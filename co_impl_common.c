#include "co.h"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


typedef struct {
    void* stack_pointer;
} CO_InternalContext;

typedef struct CO_Context {
    void* stack;
    void (*function)(void);
    size_t param_count;
    void** params;
    CO_InternalContext caller;
    CO_InternalContext callee;
    CO_ExecState run_state;
    bool dynamic_stack;
} CO_Context;

thread_local CO_Context* current_context = NULL;

/** Architecture / environment specific context switching function.
 *  It should:
 *      1) Save current state
 *      2) Save "*from"
 *      3) Restore "*to"    (switching the current state)
 *      4) Restore current state
 *      5) Call return
 */
extern void CO_impl_switch_context(CO_InternalContext* from, CO_InternalContext* to);

/** Architecture / environment specific function, that sets up the synthetic
 *  stack for the new coroutine in the way CO_impl_switch_context assumes.
 */
extern void CO_impl_init_context(
    CO_InternalContext* ctx,
    void* stack,
    size_t stack_size,
    void (*entry)(void)
);

CO_Context* CO_get_current_context() { return current_context; }

CO_ExecState CO_get_state(CO_Context* ctx) { return ctx->run_state; }

/** Creates a new context */
[[nodiscard]] static CO_Context* CO_make_context() {
    CO_Context* c = malloc(sizeof(CO_Context));
    int* new_stack_addr = malloc(STACK_SIZE);
    memset(c, 0, sizeof(CO_Context));
    c->stack = new_stack_addr;
    c->dynamic_stack = true;
    c->run_state = CO_INITIALIZED;
    c->params = NULL;
    c->param_count = 0;
    return c;
}

/** First function on the new context, sets coroutine state, calls coroutine function. */
[[noreturn]] static void CO_entry(void) {
    assert("CO_entry MUST be called from within a context!" && current_context != NULL);
    assert("CO_entry MUST NOT be called on an already started coroutine!" && current_context->run_state == CO_INITIALIZED);

    CO_Context* c = current_context;
    c->run_state = CO_RUNNING;
    switch (c->param_count) {
        case 0: c->function(); break;
        case 1: ((void (*)(void*))c->function)(c->params[0]); break;
        default: assert("More than 1 parameter is not supported for now." && false);
    }
    c->run_state = CO_FINISHED;
    CO_impl_switch_context(&c->callee, &c->caller);

    assert("This code should not be reachable!" && false);
}

/** Sets up the new context for switching into it. */
static void CO_init_context(CO_Context* ctx) {
    CO_impl_init_context(&ctx->callee, ctx->stack, STACK_SIZE, CO_entry);
}


/** Runs the provided context in a new coroutine.
 *  Returns a pointer to the created context upon leaving the function either
 *  naturally or via CO_yield, so that the execution state can be read.
 */
[[nodiscard]] CO_Context* CO_start(void (*f)(void)) {
    CO_Context* old = current_context;
    CO_Context* c = CO_make_context();
    
    c->function = f;
    CO_init_context(c);

    current_context = c;

    CO_impl_switch_context(&c->caller, &c->callee);

    current_context = old;
    return c;
}

[[nodiscard]] CO_Context* CO_start_1(void (*f)(void*), void* param1) {
    CO_Context* old = current_context;
    CO_Context* c = CO_make_context();
    
    c->function = (void (*)(void))f;
    c->param_count = 1;
    c->params = malloc(sizeof(void*));
    c->params[0] = param1;
    CO_init_context(c);

    current_context = c;

    CO_impl_switch_context(&c->caller, &c->callee);

    current_context = old;
    return c;
}

/** Hands back execution of the current coroutine to the caller to be resumed
 *  at a later time.
 */
void CO_yield() {
    CO_Context* c = current_context;
    assert("CO_yield MUST be called from within a context!" && c != NULL);
    assert("CO_yield MUST be called on a running coroutine!" && c->run_state == CO_RUNNING);
    c->run_state = CO_PAUSED;
    CO_impl_switch_context(&c->callee, &c->caller);
}

/** Continues the execution of a yielded coroutine right after the CO_yield
 *  call that paused it.
 */
void CO_continue(CO_Context* c) {
    assert("CO_continue requires a valid context." && c != NULL);
    assert("CO_continue MUST be called on a paused coroutine!" && c->run_state == CO_PAUSED);

    CO_Context* old = current_context;

    c->run_state = CO_RUNNING;

    current_context = c;

    CO_impl_switch_context(&c->caller, &c->callee);

    current_context = old;
}

void CO_restart(CO_Context* c) {
    assert("CO_restart requires a valid context." && c != NULL);
    assert("CO_restart MUST be called on a finished coroutine!" && c->run_state == CO_FINISHED);

    CO_Context* old = current_context;

    c->run_state = CO_INITIALIZED;
    CO_init_context(c);

    current_context = c;

    CO_impl_switch_context(&c->caller, &c->callee);

    current_context = old;
}

void CO_destroy_context(CO_Context* c) {
    if (c == NULL) { return; }
    assert("Nobody likes ghost processes with unfinished business..." && c->run_state == CO_FINISHED);

    // originally planned for a way to self-manage the stack allocation, that
    // did not make the cut..
    if (c->dynamic_stack) {free(c->stack);}
    if (c->param_count > 0) {free(c->params);}

    // so that we don't upset our functions with freed memory
    c->run_state = CO_UNKNOWN;
    free(c);
}
