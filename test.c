#include "co.h"
#include <stdio.h>

void print_hello() {
    for (int i = 0; i < 10; i++) {
        printf("%d\n", i);
        CO_yield();
    }
}

int main() {
    CO_Context* c1 = CO_start(print_hello);
    CO_Context* c2 = CO_start(print_hello);

    while(CO_get_state(c1) != CO_FINISHED || CO_get_state(c2) != CO_FINISHED) {
        if (CO_get_state(c1) == CO_PAUSED) {CO_continue(c1);}
        if (CO_get_state(c2) == CO_PAUSED) {CO_continue(c2);}
    }

    CO_destroy_context(c1);
    CO_destroy_context(c2);
}
