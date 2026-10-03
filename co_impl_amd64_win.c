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
	Registers* caller = &(current_context->caller_registers);

    asm volatile (
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
        "sub  $176, %%rsp;"\
        "movdqa %%xmm6,  160(%%rsp);" \
        "movdqa %%xmm7,  144(%%rsp);" \
        "movdqa %%xmm8,  128(%%rsp);" \
        "movdqa %%xmm9,  112(%%rsp);" \
        "movdqa %%xmm10,  96(%%rsp);" \
        "movdqa %%xmm11,  80(%%rsp);" \
        "movdqa %%xmm12,  64(%%rsp);" \
        "movdqa %%xmm13,  48(%%rsp);" \
        "movdqa %%xmm14,  32(%%rsp);" \
        "movdqa %%xmm15,  16(%%rsp);" \
		"stmxcsr 0(%%rsp);"
		"fnstcw 4(%%rsp);"
        "push %%rbp;" \
        "push %%rsp;" \
        "movq %%rbp, (%%rdx);" \
        "movq %%rsp, 8(%%rdx);" \

		"movq %%rsp, %%rbp;"
        "movq %%rcx, %%rsp;"    // set stack pointer to the new stack
        "call *%%rax;"         // call 'f', putting the return address as the
                            // first item on the new stack
		"movq %%rbp, %%rsp;"
        "pop  %%rsp;"
        "pop  %%rbp;"
		"ldmxcsr 0(%%rsp);"
		"fldcw 4(%%rsp);"
        "movdqa 160(%%rsp), %%xmm6;" \
        "movdqa 144(%%rsp), %%xmm7;" \
        "movdqa 128(%%rsp), %%xmm8;" \
        "movdqa 112(%%rsp), %%xmm9;" \
        "movdqa  96(%%rsp), %%xmm10;" \
        "movdqa  80(%%rsp), %%xmm11;" \
        "movdqa  64(%%rsp), %%xmm12;" \
        "movdqa  48(%%rsp), %%xmm13;" \
        "movdqa  32(%%rsp), %%xmm14;" \
        "movdqa  16(%%rsp), %%xmm15;" \
        "add $176, %%rsp;"
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
		"pop  %%rax;"
        : "+a" (f),
          "+c" (stack_top),
		  "+d" (caller)
		:
        : "xmm0",
		  "xmm1",
		  "xmm2",
		  "xmm3",
		  "xmm4",
		  "xmm5",
		  "cc",
		  "memory"
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
	Registers* caller = &(current_context->caller_registers);
	Registers* callee = &(current_context->callee_registers);

    // set state so that the others know that we yielded
    current_context->run_state = CO_PAUSED;
    // save 
    asm volatile (
		"sub $8, %%rsp;"
		"push %%rax;"
		"add $16, %%rsp;"
		"lea _continue%=(%%rip), %%rax;"
		"push %%rax;"
		"sub $8, %%rsp;"
		"mov (%%rsp), %%rax;"

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
        "sub  $176, %%rsp;"\
        "movdqa %%xmm6,  160(%%rsp);" \
        "movdqa %%xmm7,  144(%%rsp);" \
        "movdqa %%xmm8,  128(%%rsp);" \
        "movdqa %%xmm9,  112(%%rsp);" \
        "movdqa %%xmm10,  96(%%rsp);" \
        "movdqa %%xmm11,  80(%%rsp);" \
        "movdqa %%xmm12,  64(%%rsp);" \
        "movdqa %%xmm13,  48(%%rsp);" \
        "movdqa %%xmm14,  32(%%rsp);" \
        "movdqa %%xmm15,  16(%%rsp);" \
		"stmxcsr 0(%%rsp);"
		"fnstcw 4(%%rsp);"
        "push %%rbp;"
        "push %%rsp;"
        "movq %%rbp, (%%rcx);"
        "movq %%rsp, 8(%%rcx);"

        "movq 8(%%rdx), %%rbp;"
        "jmp *(%%rax);"
		"_continue%=:"
        : "+a" (ret_ptr),
		  "+c" (callee),
		  "+d" (caller)
        :
        : "xmm0",
		  "xmm1",
		  "xmm2",
		  "xmm3",
		  "xmm4",
		  "xmm5",
		  "cc",
		  "memory"
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
	Registers* caller = &(current_context->caller_registers);
	Registers* callee = &(current_context->callee_registers);
    // save our registers
    asm volatile (
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
        "sub  $176, %%rsp;"\
        "movdqa %%xmm6,  160(%%rsp);" \
        "movdqa %%xmm7,  144(%%rsp);" \
        "movdqa %%xmm8,  128(%%rsp);" \
        "movdqa %%xmm9,  112(%%rsp);" \
        "movdqa %%xmm10,  96(%%rsp);" \
        "movdqa %%xmm11,  80(%%rsp);" \
        "movdqa %%xmm12,  64(%%rsp);" \
        "movdqa %%xmm13,  48(%%rsp);" \
        "movdqa %%xmm14,  32(%%rsp);" \
        "movdqa %%xmm15,  16(%%rsp);" \
		"stmxcsr 0(%%rsp);"
		"fnstcw 4(%%rsp);"
        "push %%rbp;"
        "push %%rsp;"
        "movq %%rbp, (%%rcx);"
        "movq %%rsp, 8(%%rcx);"

        "lea _exit%=(%%rip), %%rbx;"
        "movq %%rbx, (%%rax);"
		"movq %%rsp, -8(%%rax);"

        "movq 8(%%rdx), %%rsp;"
        "movq (%%rdx),  %%rbp;"
        "pop  %%rsp;"
        "pop  %%rbp;"
		"ldmxcsr 0(%%rsp);"
		"fldcw 4(%%rsp);"
        "movdqa 160(%%rsp), %%xmm6;" \
        "movdqa 144(%%rsp), %%xmm7;" \
        "movdqa 128(%%rsp), %%xmm8;" \
        "movdqa 112(%%rsp), %%xmm9;" \
        "movdqa  96(%%rsp), %%xmm10;" \
        "movdqa  80(%%rsp), %%xmm11;" \
        "movdqa  64(%%rsp), %%xmm12;" \
        "movdqa  48(%%rsp), %%xmm13;" \
        "movdqa  32(%%rsp), %%xmm14;" \
        "movdqa  16(%%rsp), %%xmm15;" \
        "add $176, %%rsp;"
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
		"ldmxcsr 0(%%rsp);"
		"fldcw 4(%%rsp);"
        "movdqa 160(%%rsp), %%xmm6;" \
        "movdqa 144(%%rsp), %%xmm7;" \
        "movdqa 128(%%rsp), %%xmm8;" \
        "movdqa 112(%%rsp), %%xmm9;" \
        "movdqa  96(%%rsp), %%xmm10;" \
        "movdqa  80(%%rsp), %%xmm11;" \
        "movdqa  64(%%rsp), %%xmm12;" \
        "movdqa  48(%%rsp), %%xmm13;" \
        "movdqa  32(%%rsp), %%xmm14;" \
        "movdqa  16(%%rsp), %%xmm15;" \
        "add $176, %%rsp;"
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
		"pop  %%rax;"
        : "+a" (stack_top), "+c" (caller), "+d" (callee)
        :
        : "xmm0",
		  "xmm1",
		  "xmm2",
		  "xmm3",
		  "xmm4",
		  "xmm5",
		  "cc",
		  "memory"
    );

    if (current_context->run_state == CO_RUNNING) {
        current_context->run_state = CO_FINISHED;
    }
    current_context = oc;
}


void CO_restart(CO_Context* c) {
    assert("why?" && c != NULL);
    assert("Cannot restart coroutine that has not finished" && c->run_state == CO_FINISHED);

    // store "old" context
    CO_Context* oc = current_context;

    current_context = c;

    // set up context for running f
    current_context->run_state = CO_RUNNING;

	char* stack_top = ((char*)current_context->stack + STACK_SIZE - 32);      //top of stack = bottom of stack + STACK_SIZE
	Registers* caller = &(current_context->caller_registers);

    asm volatile (
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
        "sub  $176, %%rsp;"\
        "movdqa %%xmm6,  160(%%rsp);" \
        "movdqa %%xmm7,  144(%%rsp);" \
        "movdqa %%xmm8,  128(%%rsp);" \
        "movdqa %%xmm9,  112(%%rsp);" \
        "movdqa %%xmm10,  96(%%rsp);" \
        "movdqa %%xmm11,  80(%%rsp);" \
        "movdqa %%xmm12,  64(%%rsp);" \
        "movdqa %%xmm13,  48(%%rsp);" \
        "movdqa %%xmm14,  32(%%rsp);" \
        "movdqa %%xmm15,  16(%%rsp);" \
		"stmxcsr 0(%%rsp);"
		"fnstcw 4(%%rsp);"
        "push %%rbp;" \
        "push %%rsp;" \
        "movq %%rbp, (%%rdx);" \
        "movq %%rsp, 8(%%rdx);" \

		"movq %%rsp, %%rbp;"
        "movq %%rcx, %%rsp;"    // set stack pointer to the new stack
        "call *%%rax;"          // call 'f', putting the return address as the
                             // first item on the new stack
		"movq %%rbp, %%rsp;"
        "pop  %%rsp;"
        "pop  %%rbp;"
		"ldmxcsr 0(%%rsp);"
		"fldcw 4(%%rsp);"
        "movdqa 160(%%rsp), %%xmm6;" \
        "movdqa 144(%%rsp), %%xmm7;" \
        "movdqa 128(%%rsp), %%xmm8;" \
        "movdqa 112(%%rsp), %%xmm9;" \
        "movdqa  96(%%rsp), %%xmm10;" \
        "movdqa  80(%%rsp), %%xmm11;" \
        "movdqa  64(%%rsp), %%xmm12;" \
        "movdqa  48(%%rsp), %%xmm13;" \
        "movdqa  32(%%rsp), %%xmm14;" \
        "movdqa  16(%%rsp), %%xmm15;" \
        "add $176, %%rsp;"
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
        : "+a" (current_context->original_function),
          "+c" (stack_top),
		  "+d" (caller)
        :
        : "xmm0",
		  "xmm1",
		  "xmm2",
		  "xmm3",
		  "xmm4",
		  "xmm5",
		  "cc",
		  "memory"
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
