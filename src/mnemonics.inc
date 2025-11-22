static const char *mnemonics[] =
{
    "NOP\n", "LD BC, d16\n", "LD (BC), A\n", "INC BC\n", "INC B\n", "DEC B\n", "LD B, d8\n", "RLCA\n", "LD (a16), SP\n", "ADD HL, BC\n", "LD A, (BC)\n", "DEC BC\n", "INC C\n", "DEC C\n", "LD C, d8\n", "RRCA\n", 
    "STOP\n", "LD DE, d16\n", "LD (DE), A\n", "INC DE\n", "INC D\n", "DEC D\n", "LD D, d8\n", "RLA\n", "JR s8\n", "ADD HL, DE\n", "LD A, (DE)\n", "DEC DE\n", "INC E\n", "DEC E\n", "LD E, d8\n", "RRA\n", 
    "JR NZ, s8\n", "LD HL, d16\n", "LD (HL+), A\n", "INC HL\n", "INC H\n", "DEC H\n", "LD H, d8\n", "DAA\n", "JR Z, s8\n", "ADD HL, HL\n", "LD A, (HL+)\n", "DEC HL\n", "INC L\n", "DEC L\n", "LD L, d8\n", "CPL\n", 
    "JR NC, s8\n", "LD SP, d16\n", "LD (HL-), A\n", "INC SP\n", "INC (HL)\n", "DEC (HL)\n", "LD (HL), d8\n", "SCF\n", "JR C, s8\n", "ADD HL, SP\n", "LD A, (HL-)\n", "DEC SP\n", "INC A\n", "DEC A\n", "LD A, d8\n", "CCF\n", 
    "LD B, B\n", "LD B, C\n", "LD B, D\n", "LD B, E\n", "LD B, H\n", "LD B, L\n", "LD B, (HL)\n", "LD B, A\n", "LD C, B\n", "LD C, C\n", "LD C, D\n", "LD C, E\n", "LD C, H\n", "LD C, L\n", "LD C, (HL)\n", "LD C, A\n", 
    "LD D, B\n", "LD D, C\n", "LD D, D\n", "LD D, E\n", "LD D, H\n", "LD D, L\n", "LD D, (HL)\n", "LD D, A\n", "LD E, B\n", "LD E, C\n", "LD E, D\n", "LD E, E\n", "LD E, H\n", "LD E, L\n", "LD E, (HL)\n", "LD E, A\n", 
    "LD H, B\n", "LD H, C\n", "LD H, D\n", "LD H, E\n", "LD H, H\n", "LD H, L\n", "LD H, (HL)\n", "LD H, A\n", "LD L, B\n", "LD L, C\n", "LD L, D\n", "LD L, E\n", "LD L, H\n", "LD L, L\n", "LD L, (HL)\n", "LD L, A\n", 
    "LD (HL), B\n", "LD (HL), C\n", "LD (HL), D\n", "LD (HL), E\n", "LD (HL), H\n", "LD (HL), L\n", "HALT\n", "LD (HL), A\n", "LD A, B\n", "LD A, C\n", "LD A, D\n", "LD A, E\n", "LD A, H\n", "LD A, L\n", "LD A, (HL)\n", "LD A, A\n", 
    "ADD A, B\n", "ADD A, C\n", "ADD A, D\n", "ADD A, E\n", "ADD A, H\n", "ADD A, L\n", "ADD A, (HL)\n", "ADD A, A\n", "ADC A, B\n", "ADC A, C\n", "ADC A, D\n", "ADC A, E\n", "ADC A, H\n", "ADC A, L\n", "ADC A, (HL)\n", "ADC A, A\n", 
    "SUB B\n", "SUB C\n", "SUB D\n", "SUB E\n", "SUB H\n", "SUB L\n", "SUB (HL)\n", "SUB A\n", "SBC A, B\n", "SBC A, C\n", "SBC A, D\n", "SBC A, E\n", "SBC A, H\n", "SBC A, L\n", "SBC A, (HL)\n", "SBC A, A\n", 
    "AND B\n", "AND C\n", "AND D\n", "AND E\n", "AND H\n", "AND L\n", "AND (HL)\n", "AND A\n", "XOR B\n", "XOR C\n", "XOR D\n", "XOR E\n", "XOR H\n", "XOR L\n", "XOR (HL)\n", "XOR A\n", 
    "OR B\n", "OR C\n", "OR D\n", "OR E\n", "OR H\n", "OR L\n", "OR (HL)\n", "OR A\n", "CP B\n", "CP C\n", "CP D\n", "CP E\n", "CP H\n", "CP L\n", "CP (HL)\n", "CP A\n", 
    "RET NZ\n", "POP BC\n", "JP NZ, a16\n", "JP a16\n", "CALL NZ, a16\n", "PUSH BC\n", "ADD A, d8\n", "RST 0\n", "RET Z\n", "RET\n", "JP Z, a16\n", "invalid opcode\n", "CALL Z, a16\n", "CALL a16\n", "ADC A, d8\n", "RST 1\n", 
    "RET NC\n", "POP DE\n", "JP NC, a16\n", "invalid opcode\n", "CALL NC, a16\n", "PUSH DE\n", "SUB d8\n", "RST 2\n", "RET C\n", "RETI\n", "JP C, a16\n", "invalid opcode\n", "CALL C, a16\n", "invalid opcode\n", "SBC A, d8\n", "RST 3\n", 
    "LD (a8), A\n", "POP HL\n", "LD (C), A\n", "invalid opcode\n", "invalid opcode\n", "PUSH HL\n", "AND d8\n", "RST 4\n", "ADD SP, s8\n", "JP HL\n", "LD (a16), A\n", "invalid opcode\n", "invalid opcode\n", "invalid opcode\n", "XOR d8\n", "RST 5\n", 
    "LD A, (a8)\n", "POP AF\n", "LD A, (C)\n", "DI\n", "invalid opcode\n", "PUSH AF\n", "OR d8\n", "RST 6\n", "LD HL, SP+s8\n", "LD SP, HL\n", "LD A, (a16)\n", "EI\n", "invalid opcode\n", "invalid opcode\n", "CP d8\n", "RST 7\n", 
};

static const char *mnemonics2[] =
{
    "RLC B\n", "RLC C\n", "RLC D\n", "RLC E\n", "RLC H\n", "RLC L\n", "RLC (HL)\n", "RLC A\n", "RRC B\n", "RRC C\n", "RRC D\n", "RRC E\n", "RRC H\n", "RRC L\n", "RRC (HL)\n", "RRC A\n", 
    "RL B\n", "RL C\n", "RL D\n", "RL E\n", "RL H\n", "RL L\n", "RL (HL)\n", "RL A\n", "RR B\n", "RR C\n", "RR D\n", "RR E\n", "RR H\n", "RR L\n", "RR (HL)\n", "RR A\n", 
    "SLA B\n", "SLA C\n", "SLA D\n", "SLA E\n", "SLA H\n", "SLA L\n", "SLA (HL)\n", "SLA A\n", "SRA B\n", "SRA C\n", "SRA D\n", "SRA E\n", "SRA H\n", "SRA L\n", "SRA (HL)\n", "SRA A\n", 
    "SWAP B\n", "SWAP C\n", "SWAP D\n", "SWAP E\n", "SWAP H\n", "SWAP L\n", "SWAP (HL)\n", "SWAP A\n", "SRL B\n", "SRL C\n", "SRL D\n", "SRL E\n", "SRL H\n", "SRL L\n", "SRL (HL)\n", "SRL A\n", 
    "BIT 0, B\n", "BIT 0, C\n", "BIT 0, D\n", "BIT 0, E\n", "BIT 0, H\n", "BIT 0, L\n", "BIT 0, (HL)\n", "BIT 0, A\n", "BIT 1, B\n", "BIT 1, C\n", "BIT 1, D\n", "BIT 1, E\n", "BIT 1, H\n", "BIT 1, L\n", "BIT 1, (HL)\n", "BIT 1, A\n", 
    "BIT 2, B\n", "BIT 2, C\n", "BIT 2, D\n", "BIT 2, E\n", "BIT 2, H\n", "BIT 2, L\n", "BIT 2, (HL)\n", "BIT 2, A\n", "BIT 3, B\n", "BIT 3, C\n", "BIT 3, D\n", "BIT 3, E\n", "BIT 3, H\n", "BIT 3, L\n", "BIT 3, (HL)\n", "BIT 3, A\n", 
    "BIT 4, B\n", "BIT 4, C\n", "BIT 4, D\n", "BIT 4, E\n", "BIT 4, H\n", "BIT 4, L\n", "BIT 4, (HL)\n", "BIT 4, A\n", "BIT 5, B\n", "BIT 5, C\n", "BIT 5, D\n", "BIT 5, E\n", "BIT 5, H\n", "BIT 5, L\n", "BIT 5, (HL)\n", "BIT 5, A\n", 
    "BIT 6, B\n", "BIT 6, C\n", "BIT 6, D\n", "BIT 6, E\n", "BIT 6, H\n", "BIT 6, L\n", "BIT 6, (HL)\n", "BIT 6, A\n", "BIT 7, B\n", "BIT 7, C\n", "BIT 7, D\n", "BIT 7, E\n", "BIT 7, H\n", "BIT 7, L\n", "BIT 7, (HL)\n", "BIT 7, A\n", 
    "RES 0, B\n", "RES 0, C\n", "RES 0, D\n", "RES 0, E\n", "RES 0, H\n", "RES 0, L\n", "RES 0, (HL)\n", "RES 0, A\n", "RES 1, B\n", "RES 1, C\n", "RES 1, D\n", "RES 1, E\n", "RES 1, H\n", "RES 1, L\n", "RES 1, (HL)\n", "RES 1, A\n", 
    "RES 2, B\n", "RES 2, C\n", "RES 2, D\n", "RES 2, E\n", "RES 2, H\n", "RES 2, L\n", "RES 2, (HL)\n", "RES 2, A\n", "RES 3, B\n", "RES 3, C\n", "RES 3, D\n", "RES 3, E\n", "RES 3, H\n", "RES 3, L\n", "RES 3, (HL)\n", "RES 3, A\n", 
    "RES 4, B\n", "RES 4, C\n", "RES 4, D\n", "RES 4, E\n", "RES 4, H\n", "RES 4, L\n", "RES 4, (HL)\n", "RES 4, A\n", "RES 5, B\n", "RES 5, C\n", "RES 5, D\n", "RES 5, E\n", "RES 5, H\n", "RES 5, L\n", "RES 5, (HL)\n", "RES 5, A\n", 
    "RES 6, B\n", "RES 6, C\n", "RES 6, D\n", "RES 6, E\n", "RES 6, H\n", "RES 6, L\n", "RES 6, (HL)\n", "RES 6, A\n", "RES 7, B\n", "RES 7, C\n", "RES 7, D\n", "RES 7, E\n", "RES 7, H\n", "RES 7, L\n", "RES 7, (HL)\n", "RES 7, A\n", 
    "SET 0, B\n", "SET 0, C\n", "SET 0, D\n", "SET 0, E\n", "SET 0, H\n", "SET 0, L\n", "SET 0, (HL)\n", "SET 0, A\n", "SET 1, B\n", "SET 1, C\n", "SET 1, D\n", "SET 1, E\n", "SET 1, H\n", "SET 1, L\n", "SET 1, (HL)\n", "SET 1, A\n", 
    "SET 2, B\n", "SET 2, C\n", "SET 2, D\n", "SET 2, E\n", "SET 2, H\n", "SET 2, L\n", "SET 2, (HL)\n", "SET 2, A\n", "SET 3, B\n", "SET 3, C\n", "SET 3, D\n", "SET 3, E\n", "SET 3, H\n", "SET 3, L\n", "SET 3, (HL)\n", "SET 3, A\n", 
    "SET 4, B\n", "SET 4, C\n", "SET 4, D\n", "SET 4, E\n", "SET 4, H\n", "SET 4, L\n", "SET 4, (HL)\n", "SET 4, A\n", "SET 5, B\n", "SET 5, C\n", "SET 5, D\n", "SET 5, E\n", "SET 5, H\n", "SET 5, L\n", "SET 5, (HL)\n", "SET 5, A\n", 
    "SET 6, B\n", "SET 6, C\n", "SET 6, D\n", "SET 6, E\n", "SET 6, H\n", "SET 6, L\n", "SET 6, (HL)\n", "SET 6, A\n", "SET 7, B\n", "SET 7, C\n", "SET 7, D\n", "SET 7, E\n", "SET 7, H\n", "SET 7, L\n", "SET 7, (HL)\n", "SET 7, A\n", 
};

