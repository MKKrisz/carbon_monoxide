#include "co.h"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// ------------------------------------------------------
// Types
// ------------------------------------------------------

typedef struct saved_registers {
    size_t rbp;
    size_t rsp;
} Registers;


typedef struct CO_Context {
    void* stack;
    void (*original_function)(void);
    Registers caller_registers;
    Registers callee_registers;
    CO_ExecState run_state;
    bool dynamic_stack;
} CO_Context;


// ------------------------------------------------------
// Inline ASM macros
// ------------------------------------------------------

//pushes registers onto stack, saves esp, ebp to X (where X should ideally be
//somewhat compatible with saved_registers)
#define SAVE_REGISTERS(X) asm ( \
        "push %%rax;" \
        "push %%rbx;" \
        "push %%rcx;" \
        "push %%rdx;" \
        "push %%rsi;" \
        "push %%rdi;" \
        "push %%r8;" \
        "push %%r9;" \
        "push %%r10;" \
        "push %%r11;" \
        "push %%r12;" \
        "push %%r13;" \
        "push %%r14;" \
        "push %%r15;" \
        "push %%rbp;" \
        "push %%rsp;" \
        "movq %%rbp, %0;" \
        "movq %%rsp, 8+%P0;" \
        : \
        : "o" (X) \
        : "memory" \
    ) \

// sets esp, ebp to values in X (copy-paste parenthesis from SAVE_REGISTERS
// doc) and pops all registers from the freshly set stack in reverse order
// compared to how SAVED_REGISTERS pushes them.
//
// Calling this without first calling save_registers is undefined behaviour.
#define RESTORE_REGISTERS(X) asm ( \
        "movq 8+%P0, %%rsp;" \
        "movq %0,  %%rbp;" \
        "pop  %%rsp;" \
        "pop  %%rbp;" \
        "pop  %%r15;" \
        "pop  %%r14;" \
        "pop  %%r13;" \
        "pop  %%r12;" \
        "pop  %%r11;" \
        "pop  %%r10;" \
        "pop  %%r9;" \
        "pop  %%r8;" \
        "pop  %%rdi;" \
        "pop  %%rsi;" \
        "pop  %%rdx;" \
        "pop  %%rcx;" \
        "pop  %%rbx;" \
        "pop  %%rax;" \
        : \
        : "o" (X) \
        : "memory" \
    ) \

// ------------------------------------------------------
// Globals (for internal use)
// ------------------------------------------------------


// The context being executed currently.
// Only has a few reasons to exist, like it being an absolute pain to access
// it after register restoration...
thread_local CO_Context* current_context = NULL;


// ------------------------------------------------------
// Functions
// ------------------------------------------------------

CO_Context* CO_get_current_context() { return current_context; }

CO_ExecState CO_get_state(CO_Context* ctx) { return ctx->run_state; }

// Allocates a new stack and a context, initializes parameters.
CO_Context* make_context() {
    CO_Context* c = malloc(sizeof(CO_Context));
    int* new_stack_addr = malloc(STACK_SIZE * sizeof(char));
    memset(c, 0, sizeof(CO_Context));
    c->stack = new_stack_addr;
    c->dynamic_stack = true;
    c->run_state = CO_INITIALIZED;
    return c;
}


// see: interface
CO_Context* CO_start(void (*f)(void)) {
    assert("how? why?" && f != NULL);

    // store "old" context
    CO_Context* oc = current_context;

    CO_Context* c = make_context();
    current_context = c;

    // set up context for running f
    current_context->original_function = f;
    current_context->run_state = CO_RUNNING;

    // save current registers onto stack
    SAVE_REGISTERS((current_context->caller_registers));

    // hack
    asm(
        "lea %0, %%rsp;"    // set stack pointer to the new stack
        "call *%1;"         // call 'f', putting the return address as the
                            // first item on the new stack
        :
        : "m" (*((char*)current_context->stack + STACK_SIZE)),      //top of stack = bottom of stack + STACK_SIZE
          "m" (f)
        : "%rax"
    );

    RESTORE_REGISTERS((current_context->caller_registers));

    // yield sets this to CO_PAUSED, so if it is still CO_RUNNING, we finished
    if (current_context->run_state == CO_RUNNING) {
        current_context->run_state = CO_FINISHED;
    }

    // restore old context
    current_context = oc;
    return c;
}


void CO_yield() {
    assert("Cannot call CO_yield() from outside of a coroutine"
           && current_context != NULL 
           && current_context->run_state == CO_RUNNING);

    // save 
    SAVE_REGISTERS((current_context->callee_registers));
    // set state so that the others know that we yielded
    current_context->run_state = CO_PAUSED;
    asm(
        "call *%0;"         // go back to CO_start/CO_continue (notice the use of call instead of jmp)
        :
        : "m" (*((char*)current_context->stack + STACK_SIZE - sizeof(void*)))     // first thing on stack = top of stack - sizeof(ptr)
    );
}

void CO_continue(CO_Context* c) {
    assert("Not a coroutine" && c != NULL);
    assert("Cannot continue a coroutine that is not paused." && c->run_state == CO_PAUSED);

    // store old context
    CO_Context* oc = current_context;

    current_context = c;

    // reset context to running
    current_context->run_state = CO_RUNNING;

    // save our registers
    SAVE_REGISTERS((current_context->caller_registers));
    
    // restore the coroutine's registers
    RESTORE_REGISTERS((current_context->callee_registers));

    asm (
        "push %%rax;"                   // This whole segment of asm is only
        "lea _exit%=(%%rip), %%rax;"    // here so that that chain of execution
        "mov %%rax, %0;"                // remains somewhat legible. We replace
        "pop %%rax;"                    // the coroutine's return value so that
                                        // the function that returns is not 
                                        // CO_start(), but this one.
                                        //
                                        // Yes, it'd work without this, but may
                                        // cause severe headaches.


        "jmp *-136(%%rsp);"             // remember the 'call' from CO_yield()?
                                        // yes, that is exactly where it lives.

        "_exit%=:"                      // label right after jmp back.
        :
        : "m" (*((char*)current_context->stack + STACK_SIZE - sizeof(void*)))
        : "%rax"
    );

    // same ending as CO_start except return void
    RESTORE_REGISTERS((current_context->caller_registers));
    if (current_context->run_state == CO_RUNNING) {
        current_context->run_state = CO_FINISHED;
    }
    current_context = oc;
}

void CO_restart(CO_Context* c) {
    assert("how? why?" && c != NULL);

    // store "old" context
    CO_Context* oc = current_context;

    current_context = c;

    // set up context for running f
    current_context->run_state = CO_RUNNING;

    // save current registers onto stack
    SAVE_REGISTERS((current_context->caller_registers));

    // hack
    asm(
        "lea %0, %%rsp;"    // set stack pointer to the new stack
        "call *%1;"         // call 'f', putting the return address as the
                            // first item on the new stack
        :
        : "m" (*((char*)current_context->stack + STACK_SIZE)),      //top of stack = bottom of stack + STACK_SIZE
          "m" (current_context->original_function)
        : "%rax"
    );

    RESTORE_REGISTERS((current_context->caller_registers));

    // yield sets this to CO_PAUSED, so if it is still CO_RUNNING, we finished
    if (current_context->run_state == CO_RUNNING) {
        current_context->run_state = CO_FINISHED;
    }

    // restore old context
    current_context = oc;
}

void CO_destroy_context(CO_Context* c) {
    assert("why would you do that??" && c != NULL);
    assert("Nobody likes ghost processes with unfinished business..." && c->run_state == CO_FINISHED);

    // originally planned for a way to self-manage the stack allocation, that
    // did not make the cut..
    if (c->dynamic_stack) {free(c->stack);}

    // so that we don't upset our functions with freed memory
    c->run_state = CO_UNKNOWN;
    free(c);
}

#undef SAVE_REGISTERS
#undef RESTORE_REGISTERS
