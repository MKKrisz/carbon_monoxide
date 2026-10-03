.globl CO_impl_init_context
.globl CO_impl_switch_context

.type CO_impl_switch_context, @function
.type CO_impl_init_context, @function

CO_impl_init_context:
    push %rbp
    mov %rsp, %rbp
    
    lea (%rsi, %rdx), %rax
    and $-16, %rax
    sub $8, %rax
    movq %rcx,  -8(%rax)
    movq $0,   -16(%rax)
    movq $0,   -24(%rax)
    movq $0,   -32(%rax)
    movq $0,   -40(%rax)
    movq $0,   -48(%rax)
    movq $0,   -56(%rax)
    sub  $56,  %rax;
    movq %rax, (%rdi)

    pop %rbp
    ret

CO_impl_switch_context:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    movq %rsp, (%rdi)
    movq (%rsi), %rsp
    pop  %r15
    pop  %r14
    pop  %r13
    pop  %r12
    pop  %rbp
    pop  %rbx
    ret
