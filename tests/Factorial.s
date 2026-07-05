.intel_syntax noprefix
.text

factorial:
.factorialL1:
    push rbp
    mov rbp, rsp
    sub rsp, 80
    mov DWORD PTR [rbp-36], edi
    mov eax, 0
    mov DWORD PTR [rbp-8], eax
    mov eax, 0
    mov DWORD PTR [rbp-12], eax
    mov eax, 0
    mov DWORD PTR [rbp-16], eax
    mov eax, 1
    mov DWORD PTR [rbp-20], eax
    mov eax, DWORD PTR [rbp-12]
    mov DWORD PTR [rbp-24], eax
    mov eax, DWORD PTR [rbp-20]
    mov DWORD PTR [rbp-28], eax
    mov eax, DWORD PTR [rbp-16]
    mov DWORD PTR [rbp-32], eax
    jmp .factorialL2

.factorialL2:
    cmp DWORD PTR [rbp-36], 1
    jg .factorialL4
    jmp .factorialL3

.factorialL4:
    mov eax, DWORD PTR [rbp-36]
    mov DWORD PTR [rbp-40], eax
    mov eax, 0
    mov DWORD PTR [rbp-44], eax
    mov eax, DWORD PTR [rbp-40]
    mov DWORD PTR [rbp-48], eax
    mov eax, DWORD PTR [rbp-44]
    mov DWORD PTR [rbp-52], eax
    jmp .factorialL5

.factorialL5:
    cmp DWORD PTR [rbp-48], 0
    jg .factorialL7
    jmp .factorialL6

.factorialL7:
    mov eax, DWORD PTR [rbp-52]
    add eax, DWORD PTR [rbp-28]
    mov DWORD PTR [rbp-56], eax
    mov eax, DWORD PTR [rbp-56]
    mov DWORD PTR [rbp-60], eax
    mov eax, DWORD PTR [rbp-48]
    sub eax, 1
    mov DWORD PTR [rbp-64], eax
    mov eax, DWORD PTR [rbp-64]
    mov DWORD PTR [rbp-68], eax
    mov eax, DWORD PTR [rbp-68]
    mov DWORD PTR [rbp-48], eax
    mov eax, DWORD PTR [rbp-60]
    mov DWORD PTR [rbp-52], eax
    jmp .factorialL5

.factorialL6:
    mov eax, DWORD PTR [rbp-52]
    mov DWORD PTR [rbp-72], eax
    mov eax, DWORD PTR [rbp-36]
    sub eax, 1
    mov DWORD PTR [rbp-76], eax
    mov eax, DWORD PTR [rbp-76]
    mov DWORD PTR [rbp-80], eax
    mov eax, DWORD PTR [rbp-48]
    mov DWORD PTR [rbp-24], eax
    mov eax, DWORD PTR [rbp-80]
    mov DWORD PTR [rbp-36], eax
    mov eax, DWORD PTR [rbp-72]
    mov DWORD PTR [rbp-28], eax
    mov eax, DWORD PTR [rbp-52]
    mov DWORD PTR [rbp-32], eax
    jmp .factorialL2

.factorialL3:
    mov eax, DWORD PTR [rbp-28]
    mov rsp, rbp
    pop rbp
    ret


.global main
main:
.mainL1:
    push rbp
    mov rbp, rsp
    sub rsp, 8
    mov edi, 12
    call factorial
    mov DWORD PTR [rbp-8], eax
    add rsp, 0
    mov eax, DWORD PTR [rbp-8]
    mov rsp, rbp
    pop rbp
    ret


.section .note.GNU-stack, "", @progbits
