.globl CO_impl_init_context
.globl CO_impl_switch_context

.type CO_impl_switch_context, @function
.type CO_impl_init_context, @function

CO_impl_init_context:
    push %rbp
    mov %rsp, %rbp
    
    lea (%rcx, %r8), %rax
    and $-16, %rax
    sub $-32, %rax
    movq %r9,   -8(%rax)    ;ret
    movq $0,   -16(%rax)    ;rbx
    movq $0,   -24(%rax)    ;rbp
    movq $0,   -32(%rax)    ;rdi
    movq $0,   -40(%rax)    ;rsi
    movq $0,   -48(%rax)    ;r12
    movq $0,   -56(%rax)    ;r13
    movq $0,   -64(%rax)    ;r14
    movq $0,   -72(%rax)    ;r15
    movq $0,   -80(%rax)    ;xmm6h
    movq $0,   -88(%rax)    ;xmm6l
    movq $0,   -96(%rax)    ;xmm7h
    movq $0,   -104(%rax)   ;xmm7l
    movq $0,   -112(%rax)   ;xmm8h
    movq $0,   -120(%rax)   ;xmm8l
    movq $0,   -128(%rax)   ;xmm9h
    movq $0,   -136(%rax)   ;xmm9l
    movq $0,   -144(%rax)   ;xmm10h
    movq $0,   -152(%rax)   ;xmm10l
    movq $0,   -160(%rax)   ;xmm11h
    movq $0,   -168(%rax)   ;xmm11l
    movq $0,   -176(%rax)   ;xmm12h
    movq $0,   -184(%rax)   ;xmm12l
    movq $0,   -192(%rax)   ;xmm13h
    movq $0,   -200(%rax)   ;xmm13l
    movq $0,   -208(%rax)   ;xmm14h
    movq $0,   -216(%rax)   ;xmm14l
    movq $0,   -224(%rax)   ;xmm15h
    movq $0,   -232(%rax)   ;xmm15l
    sub  $232,  %rax;
    movq %rax, (%rdx)

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
    sub  $176, %%rsp
    movdqa %%xmm6,  160(%%rsp)
    movdqa %%xmm7,  144(%%rsp)
    movdqa %%xmm8,  128(%%rsp)
    movdqa %%xmm9,  112(%%rsp)
    movdqa %%xmm10,  96(%%rsp)
    movdqa %%xmm11,  80(%%rsp)
    movdqa %%xmm12,  64(%%rsp)
    movdqa %%xmm13,  48(%%rsp)
    movdqa %%xmm14,  32(%%rsp)
    movdqa %%xmm15,  16(%%rsp)
    stmxcsr 0(%%rsp)
    fnstcw 4(%%rsp)

    movq %rsp, (%rdx)

    movq (%rcx), %rsp

    ldmxcsr 0(%%rsp)
    fldcw 4(%%rsp)
    movdqa 160(%%rsp), %%xmm6
    movdqa 144(%%rsp), %%xmm7
    movdqa 128(%%rsp), %%xmm8
    movdqa 112(%%rsp), %%xmm9
    movdqa  96(%%rsp), %%xmm10
    movdqa  80(%%rsp), %%xmm11
    movdqa  64(%%rsp), %%xmm12
    movdqa  48(%%rsp), %%xmm13
    movdqa  32(%%rsp), %%xmm14
    movdqa  16(%%rsp), %%xmm15
    pop  %r15
    pop  %r14
    pop  %r13
    pop  %r12
    pop  %rsi
    pop  %rdi
    pop  %rbp
    pop  %rbx
    ret
