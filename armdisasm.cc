/******************************************************************
 * <phire> ok, I think I understand c++ template metaprogramming  *
 * <skid_au> oh                                                   *
 * <phire> that statement is going to come back and bite me later *
 ******************************************************************
 * An attempt to implement a nice sort but easy to uderstand ARM
 * disassembler (possibly) using templates.
 * Inspired by: http://www.youtube.com/watch?v=y71lli8MS8s
 */
#include <stdint.h>
#include <string>
#include <iostream>
#include <stdio.h>

// I learned this portable bitfield union hack from Bisqwit, who learned it from byuu
template<unsigned bitno, unsigned nbits=1, typename T=uint32_t>
struct BitField {
	T data;
	enum { mask = (1u << nbits) - 1u };
	template<typename T2>
	BitField& operator=(T2 val) {
		data = (data & ~(mask << bitno)) | ((nbits > 1 ? val & mask : !!val) << bitno);
		return *this;
	}
	operator unsigned() const { return (data >> bitno) & mask; }
};

typedef union {
		uint32_t raw;
		BitField<20, 8> jmptbl; /* Value to be used in jumptable */
		BitField<28, 4> cond;
		BitField< 8, 4> rotate;
		BitField< 5, 2> shift;
		BitField< 7, 5> shift_amount;
		BitField<16, 4> Mask;
		BitField< 0, 16, uint16_t> reg_list;
		/* Regs */				/* Intermedates */
		BitField<16, 4> Rn;		BitField<0, 8> imm8;
		BitField<12, 4> Rd;		BitField<0, 12, uint16_t> imm16;
		BitField< 8, 4> Rs;		BitField<0, 24, uint32_t> offset;
		BitField< 0, 4> Rm;		BitField<0, 24, uint32_t> swi_number;
		/* Coprocessor instructions */
		BitField< 0, 8> offset8;
		BitField< 0, 4> CRm;
		BitField< 8, 4> cp_num;
		BitField<12, 4> CRd;
		BitField<16, 4> CRn;
		BitField<20, 4> Opcode1;
		BitField< 5, 3> Opcode2;
} Instruction;


template<uint8_t table> 
std::string Ins(Instruction ins) {
	const int op = (table >> 1) & 0xF, s = table & 1;
	static const std::string opcodes[] = {"AND", "EOR", "SUB", "RSB", "ADD", "ADC", "SBC", "RSC",
						 		   		  "TST", "TEQ", "CMP", "CMN", "ORR", "MOV", "BIC", "MVN"}; 
	std::string opcode = opcodes[op];
	if(s) opcode = opcode + "S";
	

	return opcode;
}

std::string decode(Instruction op) {
	/* conditions */
	static const std::string conditions[] = {"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
									  		 "HI", "LS", "GE", "LT", "GT", "LE",   "", "NV"};
	std::string condition = conditions[op.cond];

	// Jumptable
	 std::string(*jmpTable[256])(Instruction)  = {
		#include "jmpTable.inc"
	};

	return jmpTable[op.jmptbl](op) + condition + "\n";

}

int main(int argc, char ** argv) {
	Instruction a;
	scanf("%x\n", &a.raw);
	std::cout << decode(a);
	return 0;
}

