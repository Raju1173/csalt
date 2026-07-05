.intel_syntax noprefix
.text

fact:
.factL1:
	push rbp
	mov rbp, rsp
	sub rsp, 48
	mov DWORD PTR [rbp-8], edi
	mov DWORD PTR [rbp-24], esi
	mov DWORD PTR [rbp-28], edx
	mov DWORD PTR [rbp-32], ecx
	mov DWORD PTR [rbp-36], r8d
	mov DWORD PTR [rbp-40], r9d
	mov eax, DWORD PTR [rbp+16]
	mov DWORD PTR [rbp-44], eax
	mov eax, DWORD PTR [rbp+24]
	mov DWORD PTR [rbp-48], eax
	cmp DWORD PTR [rbp-8], 1
	jle .factL3
	jmp .factL2

.factL3:
	mov eax, 1
	mov rsp, rbp
	pop rbp
	ret
	jmp .factL2

.factL2:
	mov eax, DWORD PTR [rbp-8]
	sub eax, 1
	mov DWORD PTR [rbp-12], eax
	mov edi, DWORD PTR [rbp-12]
	mov esi, 1
	mov edx, 2
	mov ecx, 3
	mov r8d, 4
	mov r9d, 5
	push 7
	push 6
	call fact
	mov DWORD PTR [rbp-16], eax
	add rsp, 8
	mov eax, DWORD PTR [rbp-8]
	imul eax, DWORD PTR [rbp-16]
	mov DWORD PTR [rbp-20], eax
	mov eax, DWORD PTR [rbp-20]
	mov rsp, rbp
	pop rbp
	ret


.global main
main:
.mainL1:
	push rbp
	mov rbp, rsp
	sub rsp, 8
	mov edi, 5
	mov esi, 2
	mov edx, 3
	mov ecx, 4
	mov r8d, 4
	mov r9d, 5
	push 7
	push 6
	call fact
	mov DWORD PTR [rbp-8], eax
	add rsp, 8
	mov eax, DWORD PTR [rbp-8]
	mov rsp, rbp
	pop rbp
	ret


.section .note.GNU-stack, "", @progbits
