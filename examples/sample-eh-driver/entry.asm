; entry.asm - assembled with ml64 (sandbox\scripts\eh.ps1). probe_entry(n, fn) loads a known pattern into every
; callee-saved register (rbx rbp rsi rdi r12-r15, xmm6-xmm15), calls fn(n) and restores the caller's registers.
; Its own unwind info comes from ml64.
.code
probe_entry PROC FRAME
        push rbp
        .pushreg rbp
        push rbx
        .pushreg rbx
        push rsi
        .pushreg rsi
        push rdi
        .pushreg rdi
        push r12
        .pushreg r12
        push r13
        .pushreg r13
        push r14
        .pushreg r14
        push r15
        .pushreg r15
        sub rsp, 200
        .allocstack 200
        movdqa xmmword ptr [rsp+32], xmm6
        .savexmm128 xmm6, 32
        movdqa xmmword ptr [rsp+48], xmm7
        .savexmm128 xmm7, 48
        movdqa xmmword ptr [rsp+64], xmm8
        .savexmm128 xmm8, 64
        movdqa xmmword ptr [rsp+80], xmm9
        .savexmm128 xmm9, 80
        movdqa xmmword ptr [rsp+96], xmm10
        .savexmm128 xmm10, 96
        movdqa xmmword ptr [rsp+112], xmm11
        .savexmm128 xmm11, 112
        movdqa xmmword ptr [rsp+128], xmm12
        .savexmm128 xmm12, 128
        movdqa xmmword ptr [rsp+144], xmm13
        .savexmm128 xmm13, 144
        movdqa xmmword ptr [rsp+160], xmm14
        .savexmm128 xmm14, 160
        movdqa xmmword ptr [rsp+176], xmm15
        .savexmm128 xmm15, 176
        .endprolog
        mov rbx, 1111111111111111h
        mov rbp, 2222222222222222h
        mov rsi, 3333333333333333h
        mov rdi, 4444444444444444h
        mov r12, 5555555555555555h
        mov r13, 6666666666666666h
        mov r14, 7777777777777777h
        mov r15, 0A8A8A8A8A8A8A8A8h
        mov rax, 0B6B6B6B6B6B6B6B6h
        movq xmm6, rax
        mov rax, 0B7B7B7B7B7B7B7B7h
        movq xmm7, rax
        mov rax, 0B8B8B8B8B8B8B8B8h
        movq xmm8, rax
        mov rax, 0B9B9B9B9B9B9B9B9h
        movq xmm9, rax
        mov rax, 0BABABABABABABABAh
        movq xmm10, rax
        mov rax, 0BBBBBBBBBBBBBBBBh
        movq xmm11, rax
        mov rax, 0BCBCBCBCBCBCBCBCh
        movq xmm12, rax
        mov rax, 0BDBDBDBDBDBDBDBDh
        movq xmm13, rax
        mov rax, 0BEBEBEBEBEBEBEBEh
        movq xmm14, rax
        mov rax, 0BFBFBFBFBFBFBFBFh
        movq xmm15, rax
        call rdx
        movdqa xmm6, xmmword ptr [rsp+32]
        movdqa xmm7, xmmword ptr [rsp+48]
        movdqa xmm8, xmmword ptr [rsp+64]
        movdqa xmm9, xmmword ptr [rsp+80]
        movdqa xmm10, xmmword ptr [rsp+96]
        movdqa xmm11, xmmword ptr [rsp+112]
        movdqa xmm12, xmmword ptr [rsp+128]
        movdqa xmm13, xmmword ptr [rsp+144]
        movdqa xmm14, xmmword ptr [rsp+160]
        movdqa xmm15, xmmword ptr [rsp+176]
        add rsp, 200
        pop r15
        pop r14
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        pop rbp
        ret
probe_entry ENDP
END
