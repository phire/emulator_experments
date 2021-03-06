/*
 codegen version of the c version of my arm emulator.
 Just one big switch statement, its upto the compiler to optimise this as best as it can.

 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PC 15
#define LR 14
#define SP 13

#define STORE_Rd(x) reg[RD] = x;

#define RN ((ins >> 16) &  0xf)
#define RD ((ins >> 12) &  0xf)
#define RM (ins &  0xf)
#define RS ((ins >> 8) &  0xf)
#define IMM_8 (ins & 0xff)
#define IMM_12 (ins & 0xfff)
#define IMM_SPLIT_8 ((ins & 0xf) | ((ins & 0xf00) >> 4))

#define CARRY (!!(cpsr & 0x20000000))

#define ENTER_THUMB { cpsr |= 0x20; }
#define EXIT_THUMB { cpsr &= ~0x20; }

#define RotateRight(x, y) (x >> y) | (x << ((sizeof(x)*8)-y));

#define NA printf("Instruction 0x%x at 0x%08x is not implemented\n", ins, reg[PC])

#define addFlags { c = ((signed)((Rn ^ shifter) ^ r) < 0) ^ ((signed)((Rn ^ shifter) & ~(Rn ^ shifter)) < 0); v = (signed)((Rn ^ r) & ~ (Rn ^ shifter)) < 0; }
#define subFlags { c = ((signed)((Rn ^ shifter) ^ r) < 0) ^ ((signed)((Rn ^ shifter) & (Rn ^ shifter)) < 0); v = (signed)((Rn ^ r) & (Rn ^ shifter)) < 0; }

#define ALWAYS_INLINE static inline __attribute__((always_inline))

#define ARMv5

uint32_t reg[16];
uint32_t cpsr, spsr;
uint32_t mem[0x40000];

static unsigned char test_fast_bin[] = {
  0x08, 0xd0, 0x4d, 0xe2, 0x00, 0x30, 0xa0, 0xe3, 0x04, 0x30, 0x8d, 0xe5,
  0x00, 0x30, 0x8d, 0xe5, 0x00, 0x30, 0x9d, 0xe5, 0x3c, 0x10, 0x9f, 0xe5,
  0x01, 0x00, 0x53, 0xe1, 0x0a, 0x00, 0x00, 0xca, 0x00, 0x30, 0x9d, 0xe5,
  0x04, 0x20, 0x9d, 0xe5, 0x01, 0x30, 0x83, 0xe2, 0x03, 0x30, 0x82, 0xe0,
  0x04, 0x30, 0x8d, 0xe5, 0x00, 0x30, 0x9d, 0xe5, 0x01, 0x30, 0x83, 0xe2,
  0x00, 0x30, 0x8d, 0xe5, 0x00, 0x30, 0x9d, 0xe5, 0x01, 0x00, 0x53, 0xe1,
  0xf4, 0xff, 0xff, 0xda, 0x00, 0x00, 0xa0, 0xe3, 0x08, 0xd0, 0x8d, 0xe2,
  0x1e, 0xff, 0x2f, 0xe1, 0xff, 0xe0, 0xf5, 0x05, 0x01, 0xd6, 0xa0, 0xe3,
  0xe6, 0xff, 0xff, 0xeb, 0x64, 0x00, 0x00, 0xef
}; // 1100000014 instructions, tight loop with lots of loads and stores.
const unsigned int test_fast_bin_len = 104;

// Condition bitfields, generated by gen.py
static const uint16_t conditions[] = { 0xd4aa, 0xe86a, 0xd5a6, 0xe966, 0xe6a9, 0xea69, 0xe4a5, 0xe865, 0xe89a, 0xe85a, 0xe996, 0xe956, 0xea99, 0xea59, 0xe895, 0xe855 }; 

/* GO OPTIMISER GO */

ALWAYS_INLINE void loadstore(const uint32_t switchyThing, const uint32_t ins) {
// Note LDRBT, LDRT and STRBT instructions aren't implemented (But neither is user mode)
	const uint8_t P = !!(switchyThing & 0x10), W = !!(switchyThing & 0x02),
				  U = !!(switchyThing & 0x08), Extra = ((switchyThing & 0x40) == 0);
	uint8_t I, L, Size;

	if(!Extra) {
		I = !!(switchyThing & 0x20);
		Size = (switchyThing & 0x04) ? 1 : 4;
		L = !!(switchyThing & 0x01);
	} else {
		I = !!(switchyThing & 0x04);
		if(!(ins & 0x40)) {
			Size = 2; 
			L = !!(switchyThing & 0x01);
		} else {
			if(switchyThing & 0x1) {
				Size = !(ins & 0x20) ? 1 : 2;
				L = 1;
#ifdef ARMv5E
			} else {
				Size = 8;
				L = (ins & 0x20);
#endif
			}
		}
	}

	uint32_t offset;
	if (I) { // I
		offset = reg[RM];
	} else {
		if(Extra)
			offset = IMM_SPLIT_8;
		else 
			offset = IMM_12;
	}

	uint32_t addr = reg[RN]; // base (Rn)
	if (P) { // Preincrement
		if (U) addr += offset; else addr -= offset;
	}

	if (L) { // Load or Store
		switch(Size) {
			case 1: reg[RD] = ((uint8_t  *)mem)[addr];   break;
			case 2:	reg[RD] = ((uint16_t *)mem)[addr/2]; break;
			case 4:	reg[RD] = mem[addr/4]; break;
#ifdef ARMv5E
			case 8: 
				reg[RD] = mem[addr/4];
				reg[RD | 1] = mem[addr/4 + 1];
#endif
		}
	} else {
		switch(Size) {
			case 1: ((uint8_t  *)mem)[addr]   = reg[RD] & 0xff;   break;
			case 2:	((uint16_t *)mem)[addr/2] = reg[RD] & 0xffff; break;
			case 4:	mem[addr/4] = reg[RD]; break;
#ifdef ARMv5E
			case 8:
				mem[addr/4] = reg[RD];
				mem[addr/4 + 1] = reg[(RD) | 1];
#endif
		}
	}

	if (!P) { // Post increment
		if (U) addr += offset; else	addr -= offset;
	}
	if (!P || (P && W)) // Writeback to base register (Rn)
		reg[RN] = addr;

	reg[PC] += 4;
}

ALWAYS_INLINE void loadstoreMultiple(const uint32_t switchyThing, const uint32_t ins) {
	const uint8_t P = !!(switchyThing & 0x10), U = !!(switchyThing & 0x08), 
		S = !!(switchyThing & 0x02), W = !!(switchyThing & 0x02), L = !!(switchyThing & 0x01);

	// Count number of registers to be stored
	int numregs = 0;
	uint32_t reglist = ins;
	if(!U) {
		for(int i = 0; i < 0xf; i++) {
			if (reglist & 1) numregs++;
			reglist = reglist >> 1;
		}
	}
	uint32_t Rn = reg[RN];

	uint32_t addr = Rn;
	if (!U)	addr -= numregs*4;
	if (!P) addr += 4;

	reglist = ins;
	for(int i = 0; i < 0xf; i++) {
		
		if (reglist & 1) {
			if(L) 
				reg[i] = mem[addr/4];
			else
				mem[addr/4] = reg[i];
			addr++;
			if (U) numregs++; // I don't trust the compiler to optimise two loops into one.
		}
		reglist = reglist >> 1;
	}
	
	if (W) reg[RN] = U ? Rn + numregs*4 : Rn - numregs*4;
	if (S && (ins & 0x00008000)) cpsr = spsr;
	reg[PC] += 4;
}

ALWAYS_INLINE unsigned int dataop(const uint8_t opcode, const uint32_t a, const uint32_t b) {
		switch (opcode) {
		case 0: // And
		case 8: // TST
			return a & b;
		case 1: // EOR - xor
		case 9: // TEQ
			return a ^ b;
		case 2: // Sub
		case 10: // Cmp - Compare 
			return a - b; 
		case 3: // Reverse Sub
			return b - a;
		case 4: // Add
		case 11: // CMN - Compare negated
			return a + b;
		case 5: // ADC - Add with carry
			return a + b + CARRY;
		case 6: // SBC - Substract with carry
			return (a - b) - !CARRY;
		case 7: // RSC - Reverse Substract with carry
			return (b - a) - !CARRY;
		case 12: // ORR
			return a | b;
		case 13: // Mov
			return b;
		case 14: // BIC - Bit Clear
			return a & ~b;
		case 15:
			return ~b;
		}
		return -1;
}

ALWAYS_INLINE void extraLoadStoreMultiplies(const uint8_t switchyThing, const uint32_t ins) {
	if((ins & 0x60) == 0) {
		if((switchyThing & 0xfc) == 0x0) { // Multiply MLA and MUL
			/* This one instruction mixes up the location of the Rn and Rd fields */
			uint32_t Rs = reg[RS];
			uint32_t Rm = reg[RM];

			uint32_t r;
			r = Rm * Rs;
			if(switchyThing & 0x02) { // MLA
				uint32_t Rn = reg[RD]; // Load from RD field instead of RN
				r += Rn;
			}
			reg[RN] = r; // Save to RN field instead of RD

			if(switchyThing & 0x01) { // Set flags (MLAS and MULS)
				cpsr &= 0x3fffffff; // clear old flags
				cpsr |= r & 0x7fffffff; // N
				cpsr |= (r == 0) << 30; // Z
			} 
		} else if ((switchyThing & 0xf8) == 0x1) { // Multiply (Acclumnate) long
			uint64_t r;
			if(switchyThing & 0x04) { // SMULL & SMLAL
				int32_t Rs = (int32_t) reg[RS];
				int32_t Rm = (int32_t) reg[RM];
				int64_t signedr = Rs*Rm;
				r = (uint32_t) signedr;
			} else {
				uint32_t Rs = reg[RS];
				uint32_t Rm = reg[RM];
				r = Rs*Rm;
			}

			if(switchyThing & 0x04) { // SMLAL & UMLAL 
				uint64_t acc = reg[RD] | ((uint64_t)reg[RN] << 32);
				r = r + acc;
			}
			reg[RD] = r & 0xffffffff;
			reg[RN] = (r >> 32) & 0xffffffff;

			if(switchyThing & 0x01) { // Set flags (MLALS, MULLS, SMLALS & SMULS)
				cpsr &= 0x3fffffff; // clear old flags
				cpsr |= (r & 0x7fffffffffffffff) >> 32; // N
				cpsr |= (r == 0) << 30; // Z
			} 
		} else { // Swap Byte
			uint32_t Rm = reg[RM]; 
			uint32_t Rn = reg[RN];

			uint32_t temp;
			if(switchyThing & 0x10) {
				temp = ((uint8_t*) mem)[Rn];
				((uint8_t*) mem)[Rn] = Rm & 0xff;
			} else {
				temp = mem[Rn/4];
				mem[Rn/4] = Rm;
				temp = RotateRight(temp, (Rn%4)*8);
			}
			STORE_Rd(temp);
		}
	} else return loadstore(switchyThing, ins); // All other instructions in here are load/stores
	reg[PC] += 4;
}

ALWAYS_INLINE void dataprocessing(const uint8_t switchyThing, const uint32_t ins) {
		uint32_t shifter;
		if (switchyThing & 0x20) {
			shifter = IMM_8;
			uint8_t roll = ((ins & 0xf00) >> 7);
			shifter = RotateRight(shifter, roll); 
		} else {
			uint32_t shift;
			if ((ins & 0x90) == 0x10) {
				shift = reg[RS];
			} else if ((ins & 0x90) == 0x00) {
				shift = (ins >> 7) & 0x1f;
			} else return extraLoadStoreMultiplies(switchyThing, ins);
			shifter = reg[RM];
			switch((ins >> 5) & 3) {
			case 0:
				shifter = shifter << shift; break;
			case 1:
				shifter = shifter >> shift; break;
			case 2:
				shifter = (int32_t)shifter >> shift; break;
			case 3:
				if((ins & 0x90) == 0x00 && shift == 0) {
					shifter = (shifter >> 1) | CARRY << 31; break;
				}
				shifter = RotateRight(shifter, shift); break;
			}
		}
		uint32_t Rn = reg[RN];

		const uint8_t opcode = (switchyThing >> 1) & 0xf;
		uint32_t r = dataop(opcode, Rn, shifter);
		if((opcode & 0xc) != 8) // If not comparasion operation
			STORE_Rd(r); // Store result into Rd
	
		if(switchyThing & 1) { // Store flags FIXME: still needs work
			uint8_t c = 0;
			uint8_t v = 0;
			cpsr &= 0x0fffffff; // clear old flags
			switch(opcode) {
				case 2:
				case 3:
				case 6:
				case 7:
				case 10:
					subFlags; break;
				case 4:
				case 11:
					addFlags; break;
			}
			cpsr |= r & 0x7fffffff; // N
			cpsr |= (r == 0) << 30; // Z
			cpsr |= c << 29;		// C
			cpsr |= v << 28;		// v
		}
		reg[PC] += 4;
}

ALWAYS_INLINE void statusreg(const uint8_t switchyThing, const uint32_t ins) {
	static uint32_t masks[4] = { 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

	uint32_t *sreg = (switchyThing & 4) ? &spsr : &cpsr;
	int i;
	if(switchyThing & 0x2) {
		uint32_t value;

		if(switchyThing & 0x20) {
			value = RotateRight(IMM_8, RS);
		} else {
			value = reg[RM];
		}

		uint32_t finalmask = 0;
		uint8_t mask = RN;
		for(i = 0; i < 4; i++) {
			if(mask & 1) finalmask |= masks[i];
			mask = mask >> 1;
		}
		*sreg = value & finalmask;
	} else {
		reg[RD] = *sreg;
	}
	reg[PC] += 4;
}

ALWAYS_INLINE void miscinstructions(const uint8_t switchyThing, const uint32_t ins) {
	const uint8_t bits = (switchyThing & 0x6) >> 1;
	switch((ins & 0xf0)>> 4) {
	case 0:
		return statusreg(switchyThing, ins);
	case 1:
		if(bits == 1) {
			reg[PC] = reg[RM];
#ifdef ARMv5
		} else if (bits == 3) {
			reg[RD] = __builtin_ctz(reg[RM]);
			reg[PC] += 4;
		} else {NA; exit(1);} break;
	case 3: 
		if(bits == 1) {
			reg[LR] = reg[PC] + 4;
			reg[PC] = reg[RM];
		} else {NA; exit(1);} break;
	case 7: 
		if(bits == 1) {
			reg[PC] = 0x0000000c + 8;
#endif		
		} else { NA; exit(1); } break;
	default:
		NA; exit(1);
	}
}

ALWAYS_INLINE void branch(const uint8_t switchyThing, const uint32_t ins) {
#ifdef ARMv5
	if((ins >> 28) == 0xf || switchyThing & 0x10 )
#else
	if(switchyThing & 0x10 )
#endif
	{
		reg[LR] = reg[PC] + 4;
	}
	int32_t soffset = ins << 8;
	soffset = soffset >> 6;
#ifdef ARMv5
	if((ins >> 28) == 0xf ) {
		soffset |= ((switchyThing & 0x10) != 0) << 1;
		ENTER_THUMB;
	}
#endif
	reg[PC] += soffset + 8;
}


/* M4 macros that make our life easier:
Note: if you are reading codegen.gen.c, you won't be able to see these macros.
define(`hex', `0x`'eval($1, 16)')
define(`match', `ifelse($4, `', `_match(`$1', `$2', `$3', 0, 9, 0)', `_match(`$1', `$2', `$3', `$4', `$5', 0)')')
define(`_match', `ifelse(eval(($6 & $1)==$2 && ($6 & $4)!=$5), 1,`case hex($6): `$3'(hex($6), ins); break;
			') ifelse(eval($6 < 255),1,`_match(`$1', `$2', `$3', `$4', `$5', incr($6))')')
*/
void execute() {
	for(;;) {
		const uint32_t ins = mem[(reg[PC]-8) >> 2];
		const uint8_t conditional = ins >> 28;
		const uint8_t switchyThing = (ins >> 20) & 0xff;
		if(conditional == 0xe || conditions[cpsr >> 28] & 1 << conditional){// Conditional execution
			switch(switchyThing) {
			/*  These case statements are automatically generated by m4 with 
				the makefile. See codegen.m4.c for the original file.
				`match(mask, val, func)' : func is called when (switchyThing & mask) == val
				with the optional arguments notmask and notval, the call to func can be
				supressed when (switchyThing & ~notmask) != ~notval.
			*/
			match(0xc0, 0x0, dataprocessing, 0x19, 0x10);
			match(0xf9, 0x10, miscinstructions)
			match(0xc0, 0x40, loadstore);
			match(0xfb, 0x32, statusreg); // Move to status reg
			match(0xe0, 0x80, loadstoreMultiple);
			case 0xa0 ... 0xaf:
				branch(0xa0, ins); break;
			case 0xb0 ... 0xbf:
				branch(0xb0, ins); break;
			case 0xf0 ... 0xff: // SVC
				return; // We are currently using SVC to exit the emulation loop
				reg[PC] = 0x00000008 + 8;
			default:
				NA;
				return;
			}
		} else{
			reg[PC] += 4;
		}
	}
}

int main() {
	memcpy(mem, test_fast_bin, test_fast_bin_len);
	reg[PC] = 0x5c+8;
	
	execute();
	return 0;
}
