.globl CO_impl_init_context
.globl CO_impl_switch_context

CO_impl_init_context:
    push %rbp
    mov %rsp, %rbp
    
    lea (%rdx, %r8), %rax
    and $-16, %rax
    sub $40, %rax
    movq %r9,   -8(%rax)
    movq $0,   -16(%rax)
    movq $0,   -24(%rax)
    movq $0,   -32(%rax)
    movq $0,   -40(%rax)
    movq $0,   -48(%rax)
    movq $0,   -56(%rax)
    movq $0,   -64(%rax)
    movq $0,   -72(%rax)

    movq $0,   -80(%rax)
    movq $0,   -88(%rax)

    movq $0,   -96(%rax)
    movq $0,   -104(%rax)

    movq $0,   -112(%rax)
    movq $0,   -120(%rax)

    movq $0,   -128(%rax)
    movq $0,   -136(%rax)

    movq $0,   -144(%rax)
    movq $0,   -152(%rax)

    movq $0,   -160(%rax)
    movq $0,   -168(%rax)

    movq $0,   -176(%rax)
    movq $0,   -184(%rax)

    movq $0,   -192(%rax)
    movq $0,   -200(%rax)

    movq $0,   -208(%rax)
    movq $0,   -216(%rax)

    movq $0,   -224(%rax)
    movq $0,   -232(%rax)

    movq $0,   -240(%rax)
    movq $0,   -248(%rax)

    stmxcsr -248(%rax)
    fnstcw  -244(%rax)
    sub  $248,  %rax
    movq %rax, (%rcx)

    pop %rbp
    ret

CO_impl_switch_context:
    push %rbx
    push %rbp
    push %rdi
    push %rsi
    push %r12
    push %r13
    push %r14
    push %r15
    sub  $176, %rsp
    movdqu %xmm6,  160(%rsp)
    movdqu %xmm7,  144(%rsp)
    movdqu %xmm8,  128(%rsp)
    movdqu %xmm9,  112(%rsp)
    movdqu %xmm10,  96(%rsp)
    movdqu %xmm11,  80(%rsp)
    movdqu %xmm12,  64(%rsp)
    movdqu %xmm13,  48(%rsp)
    movdqu %xmm14,  32(%rsp)
    movdqu %xmm15,  16(%rsp)
    stmxcsr 0(%rsp)
    fnstcw 4(%rsp)

    movq %rsp, (%rcx)

    movq (%rdx), %rsp

    ldmxcsr 0(%rsp)
    fldcw 4(%rsp)
    movdqu 160(%rsp), %xmm6
    movdqu 144(%rsp), %xmm7
    movdqu 128(%rsp), %xmm8
    movdqu 112(%rsp), %xmm9
    movdqu  96(%rsp), %xmm10
    movdqu  80(%rsp), %xmm11
    movdqu  64(%rsp), %xmm12
    movdqu  48(%rsp), %xmm13
    movdqu  32(%rsp), %xmm14
    movdqu  16(%rsp), %xmm15
    add  $176, %rsp
    pop  %r15
    pop  %r14
    pop  %r13
    pop  %r12
    pop  %rsi
    pop  %rdi
    pop  %rbp
    pop  %rbx
    ret
