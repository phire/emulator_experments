/*
 A version of my arm emulator that uses no code gen
 Just one big switch statement, its upto the compiler to optimise this as best as it can.

 This is mostly so I have something to compare my asm vesion with.

 I think gcc will compile it as a 256ish entry jump table with lots of conditionals down
 some of the branches for diffrent addressing modes. It will probally reach 60% the speed
 of my current assembly version with a bit of tweaking.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PC 15
#define LR 14
#define SP 13

#define NA printf("Instruction 0x%x at 0x%08x is not implemented\n", ins, reg[PC])

#define addFlags { c = ((signed)((Rn ^ shifter) ^ r) < 0) ^ ((signed)((Rn ^ shifter) & ~(Rn ^ shifter)) < 0); v = (signed)((Rn ^ r) & ~ (Rn ^ shifter)) < 0; }
#define subFlags { c = ((signed)((Rn ^ shifter) ^ r) < 0) ^ ((signed)((Rn ^ shifter) & (Rn ^ shifter)) < 0); v = (signed)((Rn ^ r) & (Rn ^ shifter)) < 0; }
/*
#define addFlags if(switchyThing & 1) { c = (((Rn ^ shifter) ^ r) < 0) ^ (((Rn ^ shifter) & ~(Rn ^ shifter)) < 0); v = ((Rn ^ r) & ~ (Rn ^ shifter)) < 0;}
#define subFlags if(switchyThing & 1) { c = (((Rn ^ shifter) ^ r) < 0) ^ (((Rn ^ shifter) & (Rn ^ shifter)) < 0); v = ((Rn ^ r) & (Rn ^ shifter)) < 0;}
*/



uint32_t reg[16];
uint32_t cpsr;
uint32_t mem[0x40000];

uint64_t count = 0;

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
};
const unsigned int test_fast_bin_len = 104;

static const uint16_t conditions[] = { 0x54aa, 0x686a, 0x55a6, 0x6966, 0x66a9, 0x6a69, 0x64a5, 0x6865, 0x689a, 0x685a, 0x6996, 0x6956, 0x6a99, 0x6a59, 0x6895, 0x6855 }; 


static inline unsigned int mathop(const uint8_t opcode, const uint32_t a, const uint32_t b) {
		switch (opcode) {
		case 0: // And
			return a & b;
		case 1: // Xor
			return a ^ b;
		case 2: // Sub
			return a - b; 
		case 4: // Add
			return a + b;
		case 10: // Cmp
			return a - b;
		case 13: // Mov
			return b;
		default:
			exit(1);
		}	
}

static inline void math(const uint32_t switchyThing, const uint32_t ins) {
		const uint8_t opcode = (switchyThing >> 1) & 0xf;

		uint32_t shifter;
		if (switchyThing & 0x20) {
			shifter = ins & 0xff;
			uint8_t roll = ((ins & 0xf00) >> 7);
			shifter = (shifter >> roll) | (shifter << (32-roll));
		} else {
			//if (ins & 0x10) 
			shifter = reg[ins & 0xf];
			//NA;
			//exit(1);
		}
		uint32_t Rn = reg[(ins >> 16) & 0xf];

		uint32_t r = mathop(opcode, Rn, shifter);
		if((opcode & 0xc) != 0x8)
			reg[(ins >> 12) & 0xf] = r; // Store result into Rd
	
		if(switchyThing & 1) { // Store flags
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
			cpsr |= r & 0xefffffff; // N
			cpsr |= (r == 0) << 30; // Z
			cpsr |= c << 29;		// C
			cpsr |= v << 28;		// v
		}
		reg[PC] += 4;
}

static inline void loadstore(const uint32_t switchyThing, const uint32_t ins) {
	if (switchyThing & 0x06) { // W || B
		NA; exit(1);
	}
	uint32_t offset;
	if (switchyThing & 0x20) { // I
		NA; exit(1);
	} else {
		offset = ins & 0xfff;
	}	

	uint32_t addr = reg[(ins >> 16) & 0xf]; // base (Rn)
	if (switchyThing & 0x10) {
		if (switchyThing & 0x08) {
			addr += offset;
		} else {
			addr -= offset;
		}
	}

	if (switchyThing & 0x01) { // L
		reg[(ins >> 12) & 0xf] = mem[addr/4];
	} else {
		mem[addr/4] = reg[(ins >> 12) & 0xf];
	}
	reg[PC] += 4;
}

void execute() {
	for(;;) {
		count++;
		uint32_t ins = mem[(reg[PC]-8) >> 2];
		uint8_t conditional = ins >> 28;
		uint8_t switchyThing = (ins >> 20) & 0xff;
		if(conditional == 0xe || conditions[cpsr >> 28] & 1 << conditional){// Conditional execution
			switch(switchyThing) {
			case 0x35:
				math(switchyThing, ins);
				break;
			case 0x08:
				math(switchyThing, ins);
				break;
			case 0x15:
				math(switchyThing, ins);
				break;
			case 0x28:
				math(switchyThing, ins);
				break;
			case 0x00 ... 0x7:
			case 0x09 ... 0xf:
			case 0x11:
			case 0x13:
			//case 0x15:
			case 0x17 ... 0x27:
			case 0x29 ... 0x2f:
			case 0x31:
			case 0x33:
		//	case 0x35:
			case 0x37 ... 0x3f: { // Thanks gcc for your lovely range operator
				math(switchyThing, ins);
				break;
			}
			case 0x12: {
				if ((ins & 0xf0) == 0x10) {
					reg[PC] = reg[ins & 0xf];
				}
				break;
			}
			case 0x58:
				loadstore(switchyThing, ins);
				break;
			case 0x59:
				loadstore(switchyThing, ins);
				break;
			case 0x40 ... 0x57: 
			case 0x5a ... 0x7f: {
				loadstore(switchyThing, ins);
				printf("0x%x\n", switchyThing);
				break;
			}
			case 0xb0 ... 0xbf: // BL
				reg[LR] = reg[PC] + 4;
			case 0xa0 ... 0xaf: {  // B
				int32_t soffset = ins << 8;
				reg[PC] += (soffset >> 6) + 8;
				break;
			}
			case 0xf0 ... 0xff: // SVC
				return;
			default:
				NA;
				return;
			}
		}else{
			reg[PC] += 4;
		}
	}
}

int main() {
	memcpy(mem, test_fast_bin, test_fast_bin_len);
	reg[PC] = 0x5c+8;
	
	execute();
	printf("%lu instructions executed\n", count);
	return 0;
}
