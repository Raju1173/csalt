.intel_syntax noprefix
.text

countpaths:
.countpathsL1:
    push rbp
    mov rbp, rsp
    sub rsp, 104
    mov DWORD PTR [rbp-44], edi
    mov eax, 0
    mov DWORD PTR [rbp-8], eax
    mov eax, 0
    mov DWORD PTR [rbp-12], eax
    mov eax, 0
    mov DWORD PTR [rbp-16], eax
    mov eax, 0
    mov DWORD PTR [rbp-20], eax
    mov eax, 0
    mov DWORD PTR [rbp-24], eax
    mov eax, 1
    mov DWORD PTR [rbp-28], eax
    mov eax, 1
    mov DWORD PTR [rbp-32], eax
    mov eax, 2
    mov DWORD PTR [rbp-36], eax
    mov eax, 3
    mov DWORD PTR [rbp-40], eax
    cmp DWORD PTR [rbp-44], 0
    je .countpathsL10
    jmp .countpathsL2

.countpathsL10:
    mov eax, DWORD PTR [rbp-28]
    mov rsp, rbp
    pop rbp
    ret
    jmp .countpathsL2

.countpathsL2:
    cmp DWORD PTR [rbp-44], 1
    je .countpathsL9
    jmp .countpathsL3

.countpathsL9:
    mov eax, DWORD PTR [rbp-32]
    mov rsp, rbp
    pop rbp
    ret
    jmp .countpathsL3

.countpathsL3:
    cmp DWORD PTR [rbp-44], 2
    je .countpathsL8
    jmp .countpathsL4

.countpathsL8:
    mov eax, DWORD PTR [rbp-36]
    mov rsp, rbp
    pop rbp
    ret
    jmp .countpathsL4

.countpathsL4:
    mov edi, DWORD PTR [rbp-48]
    call BUG
    add rsp, 0
    mov eax, DWORD PTR [rbp-20]
    mov DWORD PTR [rbp-52], eax
    mov eax, DWORD PTR [rbp-28]
    mov DWORD PTR [rbp-56], eax
    mov eax, DWORD PTR [rbp-32]
    mov DWORD PTR [rbp-60], eax
    mov eax, DWORD PTR [rbp-36]
    mov DWORD PTR [rbp-64], eax
    mov eax, DWORD PTR [rbp-40]
    mov DWORD PTR [rbp-68], eax
    jmp .countpathsL5

.countpathsL5:
    mov eax, DWORD PTR [rbp-44]
    add eax, 1
    mov DWORD PTR [rbp-72], eax
    mov eax, DWORD PTR [rbp-68]
    cmp eax, DWORD PTR [rbp-72]
    jl .countpathsL7
    jmp .countpathsL6

.countpathsL7:
    mov eax, DWORD PTR [rbp-56]
    add eax, DWORD PTR [rbp-60]
    mov DWORD PTR [rbp-76], eax
    mov eax, DWORD PTR [rbp-76]
    add eax, DWORD PTR [rbp-64]
    mov DWORD PTR [rbp-80], eax
    mov eax, DWORD PTR [rbp-80]
    mov DWORD PTR [rbp-84], eax
    mov eax, DWORD PTR [rbp-60]
    mov DWORD PTR [rbp-88], eax
    mov eax, DWORD PTR [rbp-64]
    mov DWORD PTR [rbp-92], eax
    mov eax, DWORD PTR [rbp-84]
    mov DWORD PTR [rbp-96], eax
    mov eax, DWORD PTR [rbp-68]
    add eax, 1
    mov DWORD PTR [rbp-100], eax
    mov eax, DWORD PTR [rbp-100]
    mov DWORD PTR [rbp-104], eax
    mov eax, DWORD PTR [rbp-84]
    mov DWORD PTR [rbp-52], eax
    mov eax, DWORD PTR [rbp-88]
    mov DWORD PTR [rbp-56], eax
    mov eax, DWORD PTR [rbp-92]
    mov DWORD PTR [rbp-60], eax
    mov eax, DWORD PTR [rbp-96]
    mov DWORD PTR [rbp-64], eax
    mov eax, DWORD PTR [rbp-104]
    mov DWORD PTR [rbp-68], eax
    jmp .countpathsL5

.countpathsL6:
    mov eax, DWORD PTR [rbp-52]
    mov rsp, rbp
    pop rbp
    ret


.global main
main:
.mainL1:
    push rbp
    mov rbp, rsp
    sub rsp, 8
    mov edi, 15
    call countpaths
    mov DWORD PTR [rbp-8], eax
    add rsp, 0
    mov eax, DWORD PTR [rbp-8]
    mov rsp, rbp
    pop rbp
    ret


.section .note.GNU-stack, "", @progbits
