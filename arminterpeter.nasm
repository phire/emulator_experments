; Trying to implement an arm interpeter in asm (later to be expaned
; to a jit compiler, if I don't come to my senses and decide this was
; a horriable idea.)

; Thats why its called an experment.

BITS 64

section .text
	global _start

%macro Field 3-4 4 ; Access a field from a word. Defaults to 4 bits wide
	%if %1 != %2 ; Todo can we make this use smarter movs if feilds are aligned?
		mov %1, %2
	%endif
	%if %3 > 0
		shr %1, %3
	%endif
	and %1, (1<<%4)-1
%endmacro

%macro LoadReg 2 ; Load from register (uses rbx)
	%ifid %2
		%if %2 != ebx && %2 != rbx
			mov ebx, %2
		%endif
		mov %1, dword [rsi + rbx*4]
	%else
		mov %1, dword [rsi + %2*4]
	%endif
%endmacro

%macro StoreReg 2 ; Store to register (uses rbx)
	%ifid %1
		%if %1 != ebx
			mov ebx, %1
		%endif
		mov dword [rsi + rbx*4], %2
	%else
		mov dword [rsi + %1*4], %2
	%endif
%endmacro

%macro Setbit 3
	shl %2, %3
	or %1, %2
%endmacro

%macro consertiveAlign 2 ; Aligns only if it can be done with %2 or less bytes
	%assign diff (((%1) - (($-$$) % (%1))) % (%1))
	%if diff <= %2
		times diff nop
	%endif
%endmacro

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
	Setbit r15d, eax, 31
	Setbit r15d, ebx, 30
	Setbit r15d, ecx, 29
	Setbit r15d, edx, 28
incrementProgramCounter:
	add dword [rsi + 15*4], 4 ; Increment the program counter

; Entry point
execute:
	LoadReg eax, 15 ; load program couner
	lea rax, [rax + memory]
	mov eax, [rax] ; load arm instruction
	; We do the condition check and jumptable calculations at the same time
	; For most instructions we need to do both and hopefully they 
	; will execute in parallel (TODO: benchmark this)
	mov ebx, eax 
	shr ebx, 20 - 5 ; shift and mask bits 20-27
	and rbx, 1111111100000b ; mask
	mov dl, al
	shr dl, 3 ; grab bits 4-7
	and dl, 0x1e
	or bl, dl ; and combine them with bits 20-27 to make our jump entry
	
	mov ecx, eax
	shr ecx, 28 ; condition bits
	cmp cl, 0xe
	je @true ; Always executed
	jg never ; 0xf, never executed (AKA special instructions) 
	; TODO: more conditionals
@true:
	movzx rdx, word [jmptable + rbx] ; load 16 bit value and zero extend
	lea rdx, [base + rdx]
dispatch:
	jmp rdx

base:
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
	consertiveAlign 16, 5 ; align if we are getting near the end of the instruction fetch
	fragment_%+i:
		%if i < 0x400 && i&0xe09 != 9  ; Dataprocessing instructions
		%assign opcode (i >> 5) & 0xf
		%assign S i & 0x10
		%if (!(opcode & 0xc == 0x8) || S) ; Not a Miscellaneous instruction
			; decode operands
			%if !(opcode == 0xd) && !(opcode == 0xf) ; Move/Move Not don't use Rn
				Field ecx, eax, 16 ; Rn
				LoadReg r8d, ecx ; load Rn
			%endif
			%if opcode > 0xb || opcode < 0x8 ; Test opcodes don't store a result
				Field edx, eax, 12 ; Rd
			%endif

			; The fancy shifter operand
			%if (i & 0xe00) == 0 ; Register shift
				%if (i & 0x1) ; By register
					;movzx ebx, ah
					;and bl, 0xf ; TODO: Can I build this optimisation into the macro
					Field ebx, eax, 0
					LoadReg ecx, ebx ; load Rm
					and cl, 0xf ; Mask of any extra shifting, which x86 doesn't support
				%else
					Field cx, ax, 7, 5
				%endif ; amount to shift now loaded in cl
				Field ebx, eax, 0 ; Rm
				LoadReg eax, ebx ; load Rm (value to shift)
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
				Field cl, ah, 0
				shl cl, 1
				Field eax, eax, 0, 8
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
				StoreReg edx, eax
			%endif
			%if S ; Set the flags if needed
				; Note, don't do any extra math between the main instruction and here
				jmp storeFlags
			%else
				jmp incrementProgramCounter ; increment PC and execute next instruction
			%endif
		%else ; Miscellaneous instructions
			%if i & 0x02f == 0 ; MRS
				Field ebx, eax, 12 ; Calculate register
				%if i & 0x040
					mov eax, [spsr] 
					StoreReg ebx, eax ; Save spsr into reg
				%else
					StoreReg ebx, r15d ; Save cpsr into reg
				%endif
				jmp incrementProgramCounter
			%elif i & 0x2f == 0x20 ; MSR (register operand)
				movsx ebx, al ; Calculate register (top of al is already 0)
				LoadReg r15d, ebx
				jmp incrementProgramCounter
			%else
				ret
			%endif
		%endif
		%elif i & 0xc00 == 0x400 ; load/store word/byte
			; load registers
			Field ecx, eax, 16 ; Rn
			LoadReg r8d, ecx ; load Rn
			Field edx, eax, 12 ; Rd
			%if i & 0x200 ; Intermediate

			%else ; register

			%endif

			
		%elif i & 0xe00 == 0x800 ; load store multiple
			Field ebx, eax, 16 ; Rn
			%if i &0x020 ; W (write back to register)
				push rbx ; StoreReg Rn for later
			%endif
			LoadReg ebp, ebx ; load register
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
				StoreReg ebx, edx
			%else
				LoadReg edx, ebx
				mov [rbp], edx ; store
			%endif
			%if (i & 0x080 != 0) ; U (direction)
				add ebp, 4
			%else
				sub ebp, 4
			%endif
			add rbp, 4
			jmp .loop
		.end:
			%if i &0x020 ; W (write back to register)
				pop rbx ; load Rn
				StoreReg ebx, ebp
			%endif
			jmp incrementProgramCounter

		%elif i & 0xe00 == 0xa00 ; branch instructions
			LoadReg ecx, 15 ; load PC
			shl eax, 8 ; Chop off top 8 bits,
			sar eax, 6 ; but sign extend and shift left by two
			add eax, ecx ; add offset to program counter
			add eax, 8
			StoreReg 15, eax
			%if i & 0x100 ; if L is set
				add ecx, 4 ; Calculate next address
				StoreReg 14, ecx ; and store in Link register
			%endif
			jmp execute ; next instruction
			
		%elif i & 0xf00 == 0xf00 ; Software interrupt
			; TODO: store state correctly
			StoreReg 15, 0x0000008
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
	ret

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
		dw undefined_instruction - base
	%else
		dw fragment_%+num - base ; calcualte offset to function fragment
	%endif
%assign i i+1 ; i++
%endrep

align 16
reg		dd 	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x5c

cpsr	dd	1
spsr	dd	1

align 16
memory:
	incbin "test/fast.bin"
	times 100000 db 0

