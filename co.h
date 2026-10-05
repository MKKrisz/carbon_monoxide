#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// What I want to be able to do:
//
//  - call a function
//  - whenever function reaches a specific piece of code, me, as the caller of the function, get execution back from the function, saving the state of execution.
//  - after regaining execution, be able to, at any given point, give execution back to the function, that has been suspended.
//  - the function should not notice, that it has been suspended at all.
//  - all of the above, but multithreaded
//
// Builtin C++ coroutines won't work, because:
//  - Cannot affect caller coroutine from bottom of deeply nested function calls, when some of said funcitons are not coroutines.



// STACK_SIZE macro is used to control the size of the stack allocated for each
// coroutine being run.
#ifndef STACK_SIZE
#define STACK_SIZE 4096
#endif

// Internal structure that represents a coroutine
typedef struct CO_Context CO_Context;

// Enum describing different lifecycle states of the Context object.
typedef enum CO_ExecState {
    CO_UNKNOWN = 0,             // BAD value, for BAD contexts
                                //
    CO_INITIALIZED = 1,         // State not visible to the caller, describes a
                                // context that just been initialized and is
                                // ready to receive a function.
                                //
    CO_RUNNING = 2,             // Describes a state normally only visible to
                                // the callee, where the function given as
                                // parameter for this coroutine context is
                                // being executed.
                                //
    CO_PAUSED = 3,              // State of the coroutine after calling yield()
    CO_FINISHED = 4             // State of the coroutine when exiting
                                // naturally.
} CO_ExecState;

// Returns the pointer to the currently running coroutine context, or null if a
// coroutine is not running.
CO_Context* CO_get_current_context();

// Returns the execution state of context
CO_ExecState CO_get_state(CO_Context* context);

// Creates a coroutine for the function given as parameter and starts executing
// it
[[nodiscard]] CO_Context* CO_start(void (*function)(void));

[[nodiscard]] CO_Context* CO_start_1(void (*function)(void*), void* param);

// Gives back execution to the coroutine's caller
void CO_yield();

// continues execution of a previously suspended coroutine.
void CO_continue(CO_Context* c);

// restarts a finished (naturally exited) coroutine.
void CO_restart(CO_Context* c);

// destroys a context after use. Only safe to call if the coroutine already
// finished.
void CO_destroy_context(CO_Context* c);

#ifdef __cplusplus
}
#endif

