.intel_syntax noprefix
.text

fact:
.L1:
	push rbp
	mov rbp, rsp
	sub rsp, 24
	mov DWORD PTR [rbp-24], edi
	cmp DWORD PTR [rbp-8], 1
	jle .L3
	jmp .L2

.L3:
	mov eax, 1
	mov rsp, rbp
	pop rbp
	ret
	jmp .L2

.L2:
	mov eax, DWORD PTR [rbp-8]
	sub eax, 1
	mov DWORD PTR [rbp-12], eax
	mov edi, DWORD PTR [rbp-12]
	call fact
	mov DWORD PTR [rbp-16], eax
	add rsp, 0
	mov eax, DWORD PTR [rbp-8]
	imul eax, DWORD PTR [rbp-16]
	mov DWORD PTR [rbp-20], eax
	mov eax, DWORD PTR [rbp-20]
	mov rsp, rbp
	pop rbp
	ret


.global main
main:
.L1:
	push rbp
	mov rbp, rsp
	sub rsp, 8
	mov edi, 5
	call fact
	mov DWORD PTR [rbp-8], eax
	add rsp, 0
	mov eax, DWORD PTR [rbp-8]
	mov rsp, rbp
	pop rbp
	ret


