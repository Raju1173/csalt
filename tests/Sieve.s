.intel_syntax noprefix
.text

getLastPrime:
.getLastPrimeL1:
    push rbp
    mov rbp, rsp
    sub rsp, 128
    mov DWORD PTR [rbp-24], edi
    mov eax, 0
    mov DWORD PTR [rbp-8], eax
    mov eax, 0
    mov DWORD PTR [rbp-12], eax
    mov eax, 0
    mov DWORD PTR [rbp-16], eax
    mov eax, 0
    mov DWORD PTR [rbp-20], eax
    mov eax, DWORD PTR [rbp-24]
    mov DWORD PTR [rbp-28], eax
    mov eax, 0
    mov DWORD PTR [rbp-32], eax
    mov eax, DWORD PTR [rbp-28]
    mov DWORD PTR [rbp-36], eax
    mov eax, DWORD PTR [rbp-12]
    mov DWORD PTR [rbp-40], eax
    mov eax, DWORD PTR [rbp-32]
    mov DWORD PTR [rbp-44], eax
    mov eax, DWORD PTR [rbp-16]
    mov DWORD PTR [rbp-48], eax
    jmp .getLastPrimeL2

.getLastPrimeL2:
    cmp DWORD PTR [rbp-36], 1
    jg .getLastPrimeL4
    jmp .getLastPrimeL3

.getLastPrimeL4:
    mov eax, DWORD PTR [rbp-40]
    mov DWORD PTR [rbp-52], eax
    mov eax, DWORD PTR [rbp-44]
    mov DWORD PTR [rbp-56], eax
    mov eax, DWORD PTR [rbp-48]
    mov DWORD PTR [rbp-60], eax
    cmp DWORD PTR [rbp-44], 0
    je .getLastPrimeL6
    jmp .getLastPrimeL5

.getLastPrimeL6:
    mov eax, 1
    mov DWORD PTR [rbp-64], eax
    mov eax, 2
    mov DWORD PTR [rbp-68], eax
    mov eax, DWORD PTR [rbp-68]
    mov DWORD PTR [rbp-72], eax
    mov eax, DWORD PTR [rbp-64]
    mov DWORD PTR [rbp-76], eax
    jmp .getLastPrimeL7

.getLastPrimeL7:
    mov eax, DWORD PTR [rbp-72]
    imul eax, DWORD PTR [rbp-72]
    mov DWORD PTR [rbp-80], eax
    mov eax, DWORD PTR [rbp-80]
    imul eax, 1
    mov DWORD PTR [rbp-84], eax
    mov eax, DWORD PTR [rbp-84]
    cmp eax, DWORD PTR [rbp-36]
    jle .getLastPrimeL11
    jmp .getLastPrimeL8

.getLastPrimeL11:
    mov eax, DWORD PTR [rbp-76]
    mov DWORD PTR [rbp-88], eax
    cmp DWORD PTR [rbp-76], 1
    je .getLastPrimeL13
    jmp .getLastPrimeL12

.getLastPrimeL13:
    mov eax, DWORD PTR [rbp-36]
    cdq
    idiv DWORD PTR [rbp-72]
    mov DWORD PTR [rbp-92], eax
    mov eax, DWORD PTR [rbp-92]
    imul eax, DWORD PTR [rbp-72]
    mov DWORD PTR [rbp-96], eax
    mov eax, DWORD PTR [rbp-76]
    mov DWORD PTR [rbp-100], eax
    mov eax, DWORD PTR [rbp-96]
    cmp eax, DWORD PTR [rbp-36]
    je .getLastPrimeL15
    jmp .getLastPrimeL14

.getLastPrimeL15:
    mov eax, 0
    mov DWORD PTR [rbp-104], eax
    mov eax, DWORD PTR [rbp-104]
    mov DWORD PTR [rbp-100], eax
    jmp .getLastPrimeL14

.getLastPrimeL14:
    mov eax, DWORD PTR [rbp-100]
    mov DWORD PTR [rbp-88], eax
    jmp .getLastPrimeL12

.getLastPrimeL12:
    mov eax, DWORD PTR [rbp-72]
    add eax, 1
    mov DWORD PTR [rbp-108], eax
    mov eax, DWORD PTR [rbp-108]
    mov DWORD PTR [rbp-112], eax
    mov eax, DWORD PTR [rbp-112]
    mov DWORD PTR [rbp-72], eax
    mov eax, DWORD PTR [rbp-88]
    mov DWORD PTR [rbp-76], eax
    jmp .getLastPrimeL7

.getLastPrimeL8:
    mov eax, DWORD PTR [rbp-44]
    mov DWORD PTR [rbp-116], eax
    cmp DWORD PTR [rbp-76], 1
    je .getLastPrimeL10
    jmp .getLastPrimeL9

.getLastPrimeL10:
    mov eax, DWORD PTR [rbp-36]
    mov DWORD PTR [rbp-120], eax
    mov eax, DWORD PTR [rbp-120]
    mov DWORD PTR [rbp-116], eax
    jmp .getLastPrimeL9

.getLastPrimeL9:
    mov eax, DWORD PTR [rbp-72]
    mov DWORD PTR [rbp-52], eax
    mov eax, DWORD PTR [rbp-116]
    mov DWORD PTR [rbp-56], eax
    mov eax, DWORD PTR [rbp-76]
    mov DWORD PTR [rbp-60], eax
    jmp .getLastPrimeL5

.getLastPrimeL5:
    mov eax, DWORD PTR [rbp-36]
    sub eax, 1
    mov DWORD PTR [rbp-124], eax
    mov eax, DWORD PTR [rbp-124]
    mov DWORD PTR [rbp-128], eax
    mov eax, DWORD PTR [rbp-128]
    mov DWORD PTR [rbp-36], eax
    mov eax, DWORD PTR [rbp-52]
    mov DWORD PTR [rbp-40], eax
    mov eax, DWORD PTR [rbp-56]
    mov DWORD PTR [rbp-44], eax
    mov eax, DWORD PTR [rbp-60]
    mov DWORD PTR [rbp-48], eax
    jmp .getLastPrimeL2

.getLastPrimeL3:
    mov eax, DWORD PTR [rbp-44]
    mov rsp, rbp
    pop rbp
    ret


.global main
main:
.mainL1:
    push rbp
    mov rbp, rsp
    sub rsp, 8
    mov edi, 9
    call getLastPrime
    mov DWORD PTR [rbp-8], eax
    add rsp, 0
    mov eax, DWORD PTR [rbp-8]
    mov rsp, rbp
    pop rbp
    ret


.section .note.GNU-stack, "", @progbits
