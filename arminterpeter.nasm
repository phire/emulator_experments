; Trying to implement an arm interpeter in asm (later to be expaned
; to a jit compiler, if I don't come to my sences and decide this was
; a horriable idea.)

; Thats why its called an experment.

BITS 64

section .text
	global _start

; rdx contains the 
execute:
	mov eax, [reg + 15] ; load program couner
	lea rax, [rax + memory]
	mov eax, [rax] ; load arm instruction
	mov ebx, eax 
	sar ebx, 20 - 3
	and rbx, 11111111000b
	mov rbx, [jmptable + rbx]
	jmp rbx

jmptable:
%assign i 0 
%rep 255
	%assign num i&0xf0
	dq inst%+num
%assign i i+1
%endrep

%assign i 0 
%rep 255
  %if i&0xf0 == i
	inst%+i:
		mov ecx, i
		jmp execute
  %endif
%assign i i+1
%endrep

_start:
	; We will just compile in the arm code to emulate for now. PC starts at 0
	call execute
	; Exit correctly
	mov rax,60
	mov rdi,0
	syscall

section .data

reg	times 16 dd 0h
align 16

section .bss
memory	resb	100000h



