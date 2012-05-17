; Trying to implement an arm interpeter in asm (later to be expaned
; to a jit compiler, if I don't come to my sences and decide this was
; a horriable idea.)

; Thats why its called an experment.

BITS 64

section .text
	global _start

; Map x86 flags to arm flags
;   This code is common to a large number of code fragments, so we save space
; by putting it here
storeFlags:
	; copy out all the flags before we do any math/shifting
	sets al ; N flag
	setz bl ; Z flag
	setc cl ; C flag
	seto dl ; V flag
	and r15d, 0x0fffffff ; Clear flags in MSR	
	shl eax, 31
	or r15d, eax
	shl ebx, 30
	or r15d, ebx
	shl ecx, 29
	or r15d, ecx
	shl edx, 28
	or r15d, edx
incrementProgramCounter:
	add dword [rsi + 15*4], 4 ; Increment the program counter

; Entry point
execute:
	mov eax, [rsi + 15*4] ; load program couner
	lea rax, [rax + memory]
	mov eax, [rax] ; load arm instruction
	; We do the condition check and jumptable calculations at the same time
	; For most instructions we need to do both and hopefully they 
	; will execute in parallel (TODO: benchmark this)
	mov ebx, eax 
	sar ebx, 20 - 5 ; shift and mask bits 20-27 for jump table
	and rbx, 1111111100000b ; mask
	mov dl, al
	
	mov ecx, eax
	shr ecx, 28 ; condition bits
	cmp cl, 0xe
	je @true ; Always executed
	jg never ; 0xf, never executed (AKA special instructions) 
	; TODO: more conditionals
@true:
	movzx bx, [jmptable + rbx] ; load 16 bit value and zero extend
	mov edx, eax
	mov ecx, eax ; TODO: find optimium location for these movs
off:jmp [rel+rbx]

; This macro generates a code fragment for each meaningful position in the jumptable
; Doing this as a macro allows for a compact represenation of the arm instruction set
%assign i 0 
%rep 0xfff
  %if ((i < 0xe00) || (i&0xf00 == i)) \
   && ((i&0xe00 != 0x200) || (i&0xff0 == i)) \
   && ((i&0xe00 != 0xa00) || (i&0xf00 == i)) \
   && ((i&0xe00 != 0xc00) || (i&0xff0 == i)) \
   && (!((i&0xe00 == 0) && (i & 1 == 0)) || (i&0xff7 == i)) \
   && ((i&0xe00 != 0x800) || (i&0xff0 == i)) \
   && ((i&0xe00 != 0x600) || (i&0xff7 == i)) \
   && !((i & 0xfb0 == 0x300) || (i & 0xe01 == 0x601)) 
	;align 16
	fragment_%+i:
		%if i < 0x400 && i&0xe09 != 9  ; Dataprocessing instructions
		%assign opcode (i >> 5) & 0xf
		%assign S i & 0x10
		%if (!(opcode & 0xc == 0x8) || S) ; Not a Miscellaneous instruction
			; decode operands
			%if !(opcode == 0xd) && !(opcode == 0xf) ; Move/Move Not don't use Rn
				shr ecx, 16
				and cx, 0x0f ; Rn
				mov ebx, ecx
				mov r8d, [rsi + rbx*4] ; load Rn
			%endif
			%if opcode > 0xb || opcode < 0x8 ; Test opcodes don't store a result
				shr edx, 12
				and dx, 0x0f ; Rd
			%endif

			; The fancy shifter operand
			%if (i & 0xe00) == 0 ; Register shift
				%if (i & 0x1) ; By register
					movzx ebx, ah
					and bl, 0xf
					mov ecx, [rsi + rbx*4] ; load Rm
					and cl, 0xf ; Mask of any extra shifting, which x86 doesn't support
				%else
					mov cx, ax
					shl cx, 7
					and cx, 0x1f
				%endif ; amount to shift now loaded in cl
				mov ebx, eax
				and bx, 0xf ; Rm
				mov eax, [rsi + rbx*4] ; load Rm (value to shift)
				%assign shift (i >> 1) & 3
				%if shift == 0
					shl	eax, cl
				%elif shift == 1
					shr eax, cl
				%elif shift == 2
					sar eax, cl
				%elif shift == 3
					ror eax, cl
					; fixme Implement 5.1.13 Rotate right with extend
				%endif
			%else ; intermediate 
				mov cl, ah
				and cl, 0x0f
				shl cl, 1
				and eax, 0xff
				ror eax, cl
			%endif

			%if opcode == 5 || opcode == 6 || opcode == 7 ; opcodes that use carry
				bt r15d, 29 ; set carry flag according to CPSR
			%endif

			; TODO: Make sure these all set the flags correctly
			%if opcode == 0x0 ; AND - logical AND
				and eax, r8d
			%elif opcode == 0x1 ; EOR - logical Exclusive OR
				xor eax, r8d
			%elif opcode == 0x2 ; SUB - Subtract
				sub eax, r8d
			%elif opcode == 0x3 ; RSB - Reverse Subtract
				sub r8d, eax
				mov eax, r8d
			%elif opcode == 0x4 ; ADD - Add
				add eax, r8d
			%elif opcode == 0x5 ; ADC - Add with Carry
				adc eax, r8d
			%elif opcode == 0x6 ; SBC - Subtract with Carry
				sbb eax, r8d
			%elif opcode == 0x7 ; RSC - Reverse Subtract with Carry
				sbb r8d, eax
				mov eax, r8d
			%elif opcode == 0x8 ; TST - Test
				and eax, r8d
			%elif opcode == 0x9 ; TEQ - Test Equivalence
				xor eax, r8d
			%elif opcode == 0xa ; CMP - Compare
				sub eax, r8d
			%elif opcode == 0xb ; CMN - Compare Negated
				add eax, r8d
			%elif opcode == 0xc ; ORR - Logical (inclusive) OR
				or eax, r8d
			%elif opcode == 0xd ; MOV - Move
				mov eax, r8d
			%elif opcode == 0xe ; BIC - Bit Clear
				btr eax, r8d
			%elif opcode == 0xf ; MVN - Move Not (inverse)
				mov eax, r8d
				not eax
			%endif

			; Save result back to Rd
			%if opcode > 0xb || opcode < 0x8 ; Test opcodes don't store a result
				mov ebx, edx
				mov [rsi + rbx*4], eax
			%endif
			%if S ; Set the flags if needed
				; Note, don't do any extra math between the main instruction and here
				jmp storeFlags
			%else
				jmp incrementProgramCounter ; increment PC and execute next instruction
			%endif
		%else ; Miscellaneous instruction
			%if i & 0x02f == 0 ; MRS
				mov ebx, eax
				shr ebx, 12
				and bx, 0xf ; Calculate register
				%if i & 0x040
					mov eax, [spsr] 
					mov [rsi + rbx*4], eax ; Save spsr into reg
				%else
					mov [rsi + rbx*4], r15d ; Save cpsr into reg
				%endif
				jmp incrementProgramCounter
			%elif i & 0x2f == 0x20 ; MSR (register operand)
				movsx ebx, al ; Calculate register (top of al is already 0)
				mov r15d, [rsi + rbx*4]
				jmp incrementProgramCounter
			%else
				ret
			%endif
		%endif
		%elif i & 0xe00 == 0x800 ; load store multiple
			shr ecx, 16
			and cx, 0xf ; Rn
			movzx ebx, cx 
			%if i &0x020 ; W (write back to register)
				push rbx ; Store Rn for later
			%endif
			mov ebp, [rsi + rbx*4] ; load register
			lea rbp, [rbp + memory]
			mov cx, 16
			mov ebx, 0
		.loop:
			dec cx ; deincrement counter
			jz .end 
			shr ax, 1
			jnc .loop 
			%if i & 0x010 ; L (load or store)
				mov edx, [rbp] ; load
				mov [rsi + rbx*4], edx
			%else 
				mov edx, [rsi + rbx*4] 
				mov [rbp], edx ; store
			%endif
			%if (i & 0x080 != 0) ; U (direction)
				add ebp, 4
			%else
				sub ebp, 4 ; FIXME: Chances are these are the wrong way around,
						   ; I couldn't be bothered checking
			%endif
			add rbp, 4
			jmp .loop
		.end:
			%if i &0x020 ; W (write back to register)
				pop rbx ; load Rn
				mov [rsi + rbx*4], ebp
			%endif
			jmp incrementProgramCounter

		%elif i & 0xe00 == 0xa00 ; branch instructions
			mov ecx, [esi + 15*4] ; load PC
			shl eax, 8 ; Chop off top 8 bits,
			sar eax, 6 ; but sign extend and shift left by two
			add eax, ecx ; add offset to program counter
			mov [esi + 15*4], eax ; 
			%if i & 0x100 ; if L is set
				add ecx, 4 ; Calculate next address
				mov [esi +14*4], ecx ; and store in Link register
			%endif
			jmp execute ; next instruction
			
		%elif i & 0xf00 == 0xf00 ; Software interrupt
			; TODO: store state correctly
			mov dword [rsi + 15*4], 0x0000008
			jmp execute
		%else
			ret
		%endif
  %endif
%assign i i+1
%endrep

undefined_instruction:
	ret

never:

_start:
	; We will just compile in the arm code to emulate for now. PC starts at 0
	mov rsi, reg
	call execute
	; Exit correctly
	mov rax,60
	mov rdi,0
	syscall



section .data
jmptable: ; build jumptable
%assign i 0
%assign num 0 
%rep 0xfff ; loop through all 4096 entries
	%assign num i
	%if i >= 0xe00 
		%assign num i & 0xf00
	%elif i & 0xe00 == 0x200
		%assign num i & 0xff0
	%elif i & 0xe00 == 0xa00
		%assign num i & 0xf00
	%elif i & 0xe00 == 0xc00
		%assign num i & 0xff0
	%elif (i & 0xe00 == 0) && (i & 1 == 0)
		%assign num i & 0xff7 ; filter out bottom bit of shift amount
	%elif i & 0xe00 == 0x800
		%assign num i & 0xff0
	%elif i & 0xe00 == 0x600
		%assign num i & 0xff7
	%endif
	%if (i & 0xfb0 == 0x300) || (i & 0xe01 == 0x601) ; Undefined instruction
		dw undefined_instruction - off
	%else
		dw fragment_%+num - off ; calcualte offset to function fragment
	%endif
%assign i i+1 ; i++
%endrep

cpsr	dd	1
spsr	dd	1

section .bss
reg		resd 	16
align 16
memory	resb	100000h



