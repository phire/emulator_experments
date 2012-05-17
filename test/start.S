.global _start
.extern main

.org 0
_start:
	mov		sp, #0x100000
	bl		main
	swi		#100

