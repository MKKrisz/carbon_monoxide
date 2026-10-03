#include "co.h"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


// DISCLAIMER: this is very ad-hoc code, written with a lot of AI help
// (as opposed to the linux impl, which is 100% my mistake.) The code itself
// was written by me, but there may be parts, the AI got wrong about the
// assumptions windows has about the stack.

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
//
//IF YOU CHANGE THIS CHANGE ALSO THE CO_yield CODE
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
        "push %%xmm6;" \
        "push %%xmm7;" \
        "push %%xmm8;" \
        "push %%xmm9;" \
        "push %%xmm10;" \
        "push %%xmm11;" \
        "push %%xmm12;" \
        "push %%xmm13;" \
        "push %%xmm14;" \
        "push %%xmm15;" \
        "push %%rbp;" \
        "push %%rsp;" \
        "movq %%rbp, %0;" \
        "movq %%rsp, 8+%P0;" \
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

	char* stack_top = ((char*)current_context->stack + STACK_SIZE - 32);      //top of stack = bottom of stack + STACK_SIZE

    // save current registers onto stack
    SAVE_REGISTERS((current_context->caller_registers));

    asm(
		"movq %%rsp, %%rbp;"
        "movq %1, %%rsp;"    // set stack pointer to the new stack
        "call *%0;"         // call 'f', putting the return address as the
                            // first item on the new stack
		"movq %%rbp, %%rsp;"
        "pop  %%rsp;"
        "pop  %%rbp;"
        "pop  %%xmm15;"
        "pop  %%xmm14;"
        "pop  %%xmm13;"
        "pop  %%xmm12;"
        "pop  %%xmm11;"
        "pop  %%xmm10;"
        "pop  %%xmm9;"
        "pop  %%xmm8;"
        "pop  %%xmm7;"
        "pop  %%xmm6;"
        "pop  %%r15;"
        "pop  %%r14;"
        "pop  %%r13;"
        "pop  %%r12;"
        "pop  %%r11;"
        "pop  %%r10;"
        "pop  %%r9;"
        "pop  %%r8;"
        "pop  %%rdi;"
        "pop  %%rsi;"
        "pop  %%rdx;"
        "pop  %%rcx;"
        "pop  %%rbx;"
		"pop %%rax;"
        :
        : "r" (f),
          "r" (stack_top)
        : "%rax"
    );

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

	char* ret_ptr = ((char*)current_context->stack + STACK_SIZE - 32 - sizeof(void*));  // first thing on stack = top of stack - sizeof(ptr)

    // set state so that the others know that we yielded
    current_context->run_state = CO_PAUSED;
    // save 
    asm(
		"lea _continue%=(%%rip), %%rax;"
		"push %%rax;"
		"sub $8, %%rsp;"

		// SAVE_REGISTERS except inlined
        "push %%rax;"
        "push %%rbx;"
        "push %%rcx;"
        "push %%rdx;"
        "push %%rsi;"
        "push %%rdi;"
        "push %%r8;"
        "push %%r9;"
        "push %%r10;"
        "push %%r11;"
        "push %%r12;"
        "push %%r13;"
        "push %%r14;"
        "push %%r15;"
        "push %%xmm6;"
        "push %%xmm7;"
        "push %%xmm8;"
        "push %%xmm9;"
        "push %%xmm10;"
        "push %%xmm11;"
        "push %%xmm12;"
        "push %%xmm13;"
        "push %%xmm14;"
        "push %%xmm15;"
        "push %%rbp;"
        "push %%rsp;"
        "movq %%rbp, %1;"
        "movq %%rsp, 8+%P1;"

        "movq 8+%P2, %%rbp;"
        "jmp *%0;"
		"_continue%=:"
        :
        : "m" (*ret_ptr), "m" (current_context->callee_registers), "m" (current_context->caller_registers)
		: "%rax"
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

	char* stack_top = ((char*)current_context->stack + STACK_SIZE - 32 - sizeof(void*));
    // save our registers
    asm (
        "push %%rax;"

        "lea _exit%=(%%rip), %%rax;"
        "movq %%rax, %0;"
		"movq %%rsp, %%rax;"
		"sub $120, %%rax;"
		"movq %%rax, -8%0;"

        "push %%rbx;"
        "push %%rcx;"
        "push %%rdx;"
        "push %%rsi;"
        "push %%rdi;"
        "push %%r8;"
        "push %%r9;"
        "push %%r10;"
        "push %%r11;"
        "push %%r12;"
        "push %%r13;"
        "push %%r14;"
        "push %%r15;"
        "push %%xmm6;"
        "push %%xmm7;"
        "push %%xmm8;"
        "push %%xmm9;"
        "push %%xmm10;"
        "push %%xmm11;"
        "push %%xmm12;"
        "push %%xmm13;"
        "push %%xmm14;"
        "push %%xmm15;"
        "push %%rbp;"
        "push %%rsp;"
        "movq %%rbp, %1;"
        "movq %%rsp, 8+%P1;"

        "movq 8+%P2, %%rsp;"
        "movq %2,  %%rbp;"
        "pop  %%rsp;"
        "pop  %%rbp;"
        "pop  %%xmm15;"
        "pop  %%xmm14;"
        "pop  %%xmm13;"
        "pop  %%xmm12;"
        "pop  %%xmm11;"
        "pop  %%xmm10;"
        "pop  %%xmm9;"
        "pop  %%xmm8;"
        "pop  %%xmm7;"
        "pop  %%xmm6;"
        "pop  %%r15;"
        "pop  %%r14;"
        "pop  %%r13;"
        "pop  %%r12;"
        "pop  %%r11;"
        "pop  %%r10;"
        "pop  %%r9;"
        "pop  %%r8;"
        "pop  %%rdi;"
        "pop  %%rsi;"
        "pop  %%rdx;"
        "pop  %%rcx;"
        "pop  %%rbx;"
        "pop %%rax;"

		"add $8, %%rsp;"
        "ret;"                          // remember the 'call' from CO_yield()?
                                        // yes, that is exactly where it lives.

        "_exit%=:"                      // label right after jmp back.
		"movq %%rbp, %%rsp;"
        "pop  %%rsp;"
        "pop  %%rbp;"
        "pop  %%xmm15;"
        "pop  %%xmm14;"
        "pop  %%xmm13;"
        "pop  %%xmm12;"
        "pop  %%xmm11;"
        "pop  %%xmm10;"
        "pop  %%xmm9;"
        "pop  %%xmm8;"
        "pop  %%xmm7;"
        "pop  %%xmm6;"
        "pop  %%r15;"
        "pop  %%r14;"
        "pop  %%r13;"
        "pop  %%r12;"
        "pop  %%r11;"
        "pop  %%r10;"
        "pop  %%r9;"
        "pop  %%r8;"
        "pop  %%rdi;"
        "pop  %%rsi;"
        "pop  %%rdx;"
        "pop  %%rcx;"
        "pop  %%rbx;"
		"pop %%rax;"
        :
        : "m" (*stack_top), "m" (current_context->caller_registers), "m" (current_context->callee_registers)
        : "%rax"
    );

    if (current_context->run_state == CO_RUNNING) {
        current_context->run_state = CO_FINISHED;
    }
    current_context = oc;
}


void CO_restart(CO_Context* c) {
    assert("why?" && c != NULL);

    // store "old" context
    CO_Context* oc = current_context;

    current_context = c;

    // set up context for running f
    current_context->run_state = CO_RUNNING;

	char* stack_top = ((char*)current_context->stack + STACK_SIZE - 32);      //top of stack = bottom of stack + STACK_SIZE

    // save current registers onto stack
    SAVE_REGISTERS((current_context->caller_registers));

    asm(
		"movq %%rsp, %%rbp;"
        "movq %1, %%rsp;"    // set stack pointer to the new stack
        "call *%0;"          // call 'f', putting the return address as the
                             // first item on the new stack
		"movq %%rbp, %%rsp;"
        "pop  %%rsp;"
        "pop  %%rbp;"
        "pop  %%xmm15;"
        "pop  %%xmm14;"
        "pop  %%xmm13;"
        "pop  %%xmm12;"
        "pop  %%xmm11;"
        "pop  %%xmm10;"
        "pop  %%xmm9;"
        "pop  %%xmm8;"
        "pop  %%xmm7;"
        "pop  %%xmm6;"
        "pop  %%r15;"
        "pop  %%r14;"
        "pop  %%r13;"
        "pop  %%r12;"
        "pop  %%r11;"
        "pop  %%r10;"
        "pop  %%r9;"
        "pop  %%r8;"
        "pop  %%rdi;"
        "pop  %%rsi;"
        "pop  %%rdx;"
        "pop  %%rcx;"
        "pop  %%rbx;"
		"pop %%rax;"
        :
        : "r" (current_context->original_function),
          "r" (stack_top)
        : "%rax"
    );

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
