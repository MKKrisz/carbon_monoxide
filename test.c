#include "co.h"
#include <assert.h>
#include <stdio.h>

void co_test1_1() {
    printf("I am alive!\n");
    CO_Context* ctx = CO_get_current_context();
    assert("I should be dead..." && CO_get_state(ctx) == CO_RUNNING);
}

void test1() {
    printf("\nStarting co_test1_1\n\n");
    CO_Context* ctx = CO_start(co_test1_1);
    assert("State not set to finished" && CO_get_state(ctx) == CO_FINISHED);
    assert("Still running context reported" && CO_get_current_context() == nullptr);
    CO_destroy_context(ctx);
}

void co_test2_1() {
    printf("I am alive!\n");
    CO_Context* ctx = CO_get_current_context();
    assert("Overtaking the main stack initiated" && ctx != nullptr);
    assert("I should be dead..." && CO_get_state(ctx) == CO_RUNNING);
    CO_yield();
    printf("I am back!\n");
    ctx = CO_get_current_context();
    assert("Overtaking the main stack initiated" && ctx != nullptr);
    assert("I should be dead..." && CO_get_state(ctx) == CO_RUNNING);
}

void test2() {
    printf("\nStarting co_test2_1\n\n");
    CO_Context* ctx = CO_start(co_test2_1);
    assert("State not set to paused" && CO_get_state(ctx) == CO_PAUSED);
    assert("Still running context reported" && CO_get_current_context() == nullptr);
    CO_continue(ctx);
    assert("State not set to finished" && CO_get_state(ctx) == CO_FINISHED);
    assert("Still running context reported" && CO_get_current_context() == nullptr);
    CO_destroy_context(ctx);
}

void co_test3_1() {
    printf("I am alive! ");
    for (int i = 0; i < 10; i++) {
        printf("%d\n", i);
        CO_yield();
        printf("I am back! ");
    }
    printf("\nI am tired\n");
}

void test3() {
    printf("\nStarting co_test3_1\n\n");
    CO_Context* ctx = CO_start(co_test3_1);
    while(CO_get_state(ctx) == CO_PAUSED) {
        CO_continue(ctx);
    }

    CO_destroy_context(ctx);
}

void test4() {
    printf("\nStarting 4 co_test3_1 interleaved\n\n");
    CO_Context* ctx1 = CO_start(co_test3_1);
    CO_Context* ctx2 = CO_start(co_test3_1);
    CO_Context* ctx3 = CO_start(co_test3_1);
    CO_Context* ctx4 = CO_start(co_test3_1);
    CO_ExecState ctx1_old = CO_get_state(ctx1);
    CO_ExecState ctx2_old = CO_get_state(ctx2);
    CO_ExecState ctx3_old = CO_get_state(ctx3);
    CO_ExecState ctx4_old = CO_get_state(ctx4);

    while(CO_get_state(ctx1) == CO_PAUSED || CO_get_state(ctx2) == CO_PAUSED) {
        CO_ExecState ctx1_current = CO_get_state(ctx1);
        CO_ExecState ctx2_current = CO_get_state(ctx2);
        CO_ExecState ctx3_current = CO_get_state(ctx3);
        CO_ExecState ctx4_current = CO_get_state(ctx4);
        if (ctx1_old != ctx1_current) { printf("1 state change: %d -> %d\n", ctx1_old, ctx1_current); ctx1_old = ctx1_current; }
        if (ctx2_old != ctx2_current) { printf("2 state change: %d -> %d\n", ctx2_old, ctx2_current); ctx2_old = ctx2_current; }
        if (ctx3_old != ctx3_current) { printf("3 state change: %d -> %d\n", ctx3_old, ctx3_current); ctx3_old = ctx3_current; }
        if (ctx4_old != ctx4_current) { printf("4 state change: %d -> %d\n", ctx4_old, ctx4_current); ctx4_old = ctx4_current; }
        if (ctx1_current == CO_PAUSED) {CO_continue(ctx1);}
        if (ctx2_current == CO_PAUSED) {CO_continue(ctx2);}
        if (ctx3_current == CO_PAUSED) {CO_continue(ctx3);}
        if (ctx4_current == CO_PAUSED) {CO_continue(ctx4);}
        ctx1_current = CO_get_state(ctx1);
        ctx2_current = CO_get_state(ctx2);
        ctx3_current = CO_get_state(ctx3);
        ctx4_current = CO_get_state(ctx4);
        if (ctx1_old != ctx1_current) { printf("1 state change: %d -> %d\n", ctx1_old, ctx1_current); ctx1_old = ctx1_current; }
        if (ctx2_old != ctx2_current) { printf("2 state change: %d -> %d\n", ctx2_old, ctx2_current); ctx2_old = ctx2_current; }
        if (ctx3_old != ctx3_current) { printf("3 state change: %d -> %d\n", ctx3_old, ctx3_current); ctx3_old = ctx3_current; }
        if (ctx4_old != ctx4_current) { printf("4 state change: %d -> %d\n", ctx4_old, ctx4_current); ctx4_old = ctx4_current; }
    }

    printf("\nAll finished.\n");

    CO_destroy_context(ctx1);
    CO_destroy_context(ctx2);
    CO_destroy_context(ctx3);
    CO_destroy_context(ctx4);
}

void test5() {
    printf("\nStarting co_test3_1\n\n");
    CO_Context* ctx = CO_start(co_test3_1);
    while(CO_get_state(ctx) == CO_PAUSED) {
        CO_continue(ctx);
    }
    printf("\nRestarting co_test3\n\n");
    CO_restart(ctx);
    while(CO_get_state(ctx) == CO_PAUSED) {
        CO_continue(ctx);
    }
    printf("\nFinished.\n");
}

int main() {
    test1();
    test2();
    test3();
    test4();
    test5();
}
