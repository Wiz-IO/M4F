// ARMv7-M / Cortex M4F / 16 битови инструкции

#include "M4.h"
#include "common.h"

// GROUP 0 ////////////////////////////

int execute_0_shift(void)
{
    FUNC_VM();

    uint32_t imm5 = (CPU.op >> 6) & 0x1F;
    uint32_t value = CPU.REG.r[(CPU.op >> 3) & 0x7]; // [Rm]
    uint32_t result;

    switch ((CPU.op >> 11) & 0x3) // op_type
    {
    case 0: // LSL Rd, Rm, # [000 00 # Rm Rd]
        result = value << imm5;
        break;
    case 1: // LSR Rd, Rm, # [000 01 # Rm Rd]
        result = value >> imm5;
        break;
    case 2: // ASR Rd, Rm, # [000 10 # Rm Rd]
        result = (int32_t)value >> imm5;
        break;
    default:
        return -1;
    }

    CPU.REG.r[CPU.op & 0x7] = result; // [Rd]

    // update_flags(); // Update Z, N, C flags
    return 0;
}

int execute_0_add_sub_imm(void)
{
    FUNC_VM();
    uint32_t imm3 = (CPU.op >> 6) & 0x7;
    uint32_t op = (CPU.op >> 9) & 0x1;
    uint32_t value = CPU.REG.r[(CPU.op >> 3) & 0x7]; // [Rn]
    uint32_t result;

    if (op == 0)
    { // ADD Rd, Rn, # [000 1110 # Rn Rd]
        result = value + imm3;
    }
    else
    { // SUB Rd, Rn, # [000 1111 # Rn Rd]
        result = value - imm3;
    }

    CPU.REG.r[CPU.op & 0x7] = result; // [Rd]

    // update_flags(); // Update Z, N, C, V flags
    return 0;
}

int execute_0_add_sub_reg(void)
{
    FUNC_VM();
    uint32_t value1 = CPU.REG.r[(CPU.op >> 3) & 0x7]; // [Rn]
    uint32_t value2 = CPU.REG.r[(CPU.op >> 6) & 0x7]; // [Rm]
    uint32_t result;

    if (((CPU.op >> 9) & 0x1) == 0)
    { // ADD Rd, Rn, Rm  [000 1100 Rm Rn Rd]
        result = value1 + value2;
    }
    else
    { // SUB Rd, Rn, Rm [000 1101 Rm Rn Rd]
        result = value1 - value2;
    }

    CPU.REG.r[CPU.op & 0x7] = result; // [Rd]

    // update_flags(); // Update Z, N, C, V flags
    return 0;
}

static int execute_0(void)
{
    FUNC_VM();

    if (CPU.op >> 11 == 3) // ADD/SUB
    {

        if (CPU.op & 0b0000010000000000) // bit 10 0x400
        {
            return execute_0_add_sub_imm();
        }
        else
        {
            return execute_0_add_sub_reg();
        }
    }
    else
    {
        return execute_0_shift();
    }

    return -1;
}

// GROUP 1 ////////////////////////////

static int execute_1_mov(void)
{ // MOV Rd, # [001 00 Rd #]
    FUNC_VM();
    uint32_t imm8 = CPU.op & 0xFF;         // imm8
    CPU.REG.r[(CPU.op >> 8) & 0x7] = imm8; // Запис в [Rd] (R0–R7)
    CPU.psr.apsr.Z = (imm8 == 0);          // Zero флаг
    CPU.psr.apsr.N = 0;                    // Negative флаг (винаги 0 за imm8) ???
    return 0;
}

static int execute_1_cmp(void)
{ // CMP Rn, # [001 01 Rn #]
    FUNC_VM();
    uint32_t imm8 = CPU.op & 0xFF;                                // imm8
    uint32_t value = CPU.REG.r[(CPU.op >> 8) & 0x7];              // Стойност на [Rn] (R0–R7)
    uint32_t result = value - imm8;                               // Изваждане (само за флагове)
    CPU.psr.apsr.Z = (result == 0);                               // Zero флаг
    CPU.psr.apsr.N = (result >> 31) & 0x1;                        // Negative флаг
    CPU.psr.apsr.C = (value >= imm8);                             // Carry флаг (беззнаково)
    CPU.psr.apsr.V = ((value ^ result) & (~imm8 ^ result)) >> 31; // Overflow флаг
    return 0;
}

static int execute_1_add(void)
{ // ADD Rd, # [001 10 Rd #]
    FUNC_VM();
    uint32_t rd = (CPU.op >> 8) & 0x7;                           // Rd (R0–R7)
    uint32_t imm8 = CPU.op & 0xFF;                               // imm8
    uint32_t value = CPU.REG.r[rd];                              // Стойност на Rd
    uint32_t result = value + imm8;                              // Добавяне
    CPU.REG.r[rd] = result;                                      // Запис в Rd
    CPU.psr.apsr.Z = (result == 0);                              // Zero флаг
    CPU.psr.apsr.N = (result >> 31) & 0x1;                       // Negative флаг
    CPU.psr.apsr.C = ((uint64_t)value + imm8 > 0xFFFFFFFF);      // Carry флаг
    CPU.psr.apsr.V = ((value ^ result) & (imm8 ^ result)) >> 31; // Overflow флаг
    return 0;
}

static int execute_1_sub(void)
{ // SUB Rd, # [001 11 Rd #]
    FUNC_VM();
    uint32_t rd = (CPU.op >> 8) & 0x7;                            // Rd (R0–R7)
    uint32_t imm8 = CPU.op & 0xFF;                                // imm8
    uint32_t value = CPU.REG.r[rd];                               // Стойност на Rd
    uint32_t result = value - imm8;                               // Изваждане
    CPU.REG.r[rd] = result;                                       // Запис в Rd
    CPU.psr.apsr.Z = (result == 0);                               // Zero флаг
    CPU.psr.apsr.N = (result >> 31) & 0x1;                        // Negative флаг
    CPU.psr.apsr.C = (value >= imm8);                             // Carry флаг
    CPU.psr.apsr.V = ((value ^ result) & (~imm8 ^ result)) >> 31; // Overflow флаг
    return 0;
}

static int execute_1(void)
{
    FUNC_VM();
    // MOV/CMP/ADD/SUB imm
    switch ((CPU.op >> 11) & 3)
    {
    case 0:
        return execute_1_mov();
    case 1:
        return execute_1_cmp();
    case 2:
        return execute_1_add();
    case 3:
        return execute_1_sub();
    }
    return -1;
}

// GROUP 2 ////////////////////////////
/*                      [000 0000000 000000]
    AND Rd, Rm          [010 0000000 Rm Rd] >>6
    EOR Rd, Rm          [010 0000001 Rm Rd]
    LSL Rd, Rs          [010 0000010 Rs Rd]
    LSR Rd, Rs          [010 0000011 Rs Rd]
    ASR Rd, Rs          [010 0000100 Rs Rd]
    ADC Rd, Rm          [010 0000101 Rm Rd]
    SBC Rd, Rm          [010 0000110 Rm Rd]
    ROR Rd, Rs          [010 0000111 Rs Rd]
    TST Rm, Rn          [010 0001000 Rn Rm]
    NEG Rd, Rm          [010 0001001 Rm Rd]
    CMP Rm, Rn          [010 0001010 Rn Rm]
    CMN Rm, Rn          [010 0001011 Rn Rm]
    ORR Rd, Rm          [010 0001100 Rm Rd]
    MUL Rd, Rm          [010 0001101 Rm Rd]
    BIC Rm, Rd          [010 0001110 Rn Rm]
    MVN Rd, Rm          [010 0001111 Rm Rd]
    BX Rm               [010 001110 H2 Rm 0 0 0] >>7
    BLX Rm              [010 001111 H2 Rm 0 0 0]
    ADD Rd, Rm          [010 00100 H1H2 Rm Rd] >> 8
    CMP Rm, Rn          [010 00101 H1H2 Rn Rm]
    MOV Rd, Rm          [010 00110 H1H2 Rm Rd]
    STR Rd, [Rn, Rm]    [010 1000 Rm Rn Rd] >> 9
    STRH Rd, [Rn, Rm]   [010 1001 Rm Rn Rd]
    STRB Rd, [Rn, Rm]   [010 1010 Rm Rn Rd]
    LDRSB Rd, [Rn, Rm]  [010 1011 Rm Rn Rd]
    LDR Rd, [Rn, Rm]    [010 1100 Rm Rn Rd]
    LDRH Rd, [Rn, Rm]   [010 1101 Rm Rn Rd]
    LDRB Rd, [Rn, Rm]   [010 1110 Rm Rn Rd]
    LDRSH Rd, [Rn, Rm]  [010 1111 Rm Rn Rd]
    LDR Rd, [PC, #]     [010 01 Rd PC Relative Offset] >> 10
*/

// AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR, TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN
int execute_2_and_rd_rm(void)
{
    FUNC_VM();
    uint32_t rm = (CPU.op >> 3) & 0x7; // Rm или Rn
    uint32_t rd = CPU.op & 0x7;        // Rd или Rm
    uint32_t value1 = CPU.REG.r[rd];   // Rd или Rn
    uint32_t value2 = CPU.REG.r[rm];   // Rm или Rs
    uint32_t result;
    uint32_t carry = CPU.psr.apsr.C;

    switch ((CPU.op >> 6) & 0xF)
    {         // Битове 9:6
    case 0x0: // AND Rd, Rm
        result = value1 & value2;
        // update_flags();
        break;
    case 0x1: // EOR Rd, Rm
        result = value1 ^ value2;
        // update_flags();
        break;
    case 0x2: // LSL Rd, Rs
        if (value2 == 0)
        {
            result = value1;
        }
        else
        {
            carry = (value1 >> (32 - (value2 & 0xFF))) & 0x1;
            result = value1 << (value2 & 0xFF);
        }
        // update_flags();
        break;
    case 0x3: // LSR Rd, Rs
        if (value2 == 0)
        {
            result = value1;
        }
        else
        {
            carry = (value1 >> ((value2 & 0xFF) - 1)) & 0x1;
            result = value1 >> (value2 & 0xFF);
        }
        // update_flags();
        break;
    case 0x4: // ASR Rd, Rs
        if (value2 == 0)
        {
            result = value1;
        }
        else
        {
            carry = (value1 >> ((value2 & 0xFF) - 1)) & 0x1;
            result = (int32_t)value1 >> (value2 & 0xFF);
        }
        // update_flags();
        break;
    case 0x5: // ADC Rd, Rm
        result = value1 + value2 + carry;
        // update_flags();
        break;
    case 0x6: // SBC Rd, Rm
        result = value1 - value2 - (1 - carry);
        // update_flags();
        break;
    case 0x7: // ROR Rd, Rs
        if (value2 == 0)
        {
            result = value1;
        }
        else
        {
            uint32_t shift = value2 & 0xFF;
            carry = (value1 >> (shift - 1)) & 0x1;
            result = (value1 >> shift) | (value1 << (32 - shift));
        }
        // update_flags();
        break;
    case 0x8: // TST Rn, Rm
        result = value1 & value2;
        // update_flags();
        return 0;
    case 0x9: // NEG Rd, Rm
        result = 0 - value2;
        // update_flags();
        break;
    case 0xA: // CMP Rn, Rm
        result = value1 - value2;
        // update_flags();
        return 0;
    case 0xB: // CMN Rn, Rm
        result = value1 + value2;
        // update_flags();
        return 0;
    case 0xC: // ORR Rd, Rm
        result = value1 | value2;
        // update_flags();
        break;
    case 0xD: // MUL Rd, Rm
        result = value1 * value2;
        // update_flags();
        break;
    case 0xE: // BIC Rd, Rm
        result = value1 & ~value2;
        // update_flags();
        break;
    case 0xF: // MVN Rd, Rm
        result = ~value2;
        // update_flags();
        break;
    default:
        return -1;
    }

    CPU.REG.r[rd] = result;
    return 0;
}

// BX Rm, BLX Rm
int execute_2_bx_rm(void)
{
    FUNC_VM();
    uint32_t rm = (CPU.op >> 3) & 0xF; // Rm (вкл. H2)
    uint32_t h2 = (CPU.op >> 6) & 0x1; // H2
    uint32_t rm_idx = rm | (h2 << 3);  // Rm или висок регистър
    uint32_t target = CPU.REG.r[rm_idx];

    if ((CPU.op >> 6) & 0x1)
    {                                        // BLX
        CPU.REG.LR = (CPU.REG.PC + 2) | 0x1; // Запазване на следващия адрес с Thumb бит
    }
    CPU.REG.PC = target & ~0x1; // Смяна на PC, изчистване на Thumb бит
    return 0;
}

// ADD Rd, Rm, CMP Rm, Rn, MOV Rd, Rm
int execute_2_add_rd_rm(void)
{
    FUNC_VM();
    uint32_t rm = (CPU.op >> 3) & 0xF; // Rm (вкл. H2)
    uint32_t rd = CPU.op & 0x7;        // Rd (вкл. H1)
    uint32_t h1 = (CPU.op >> 7) & 0x1; // H1
    uint32_t h2 = (CPU.op >> 6) & 0x1; // H2
    uint32_t rd_idx = rd | (h1 << 3);  // Rd или висок регистър
    uint32_t rm_idx = rm | (h2 << 3);  // Rm или висок регистър
    uint32_t value1 = CPU.REG.r[rd_idx];
    uint32_t value2 = CPU.REG.r[rm_idx];
    uint32_t result;

    switch ((CPU.op >> 8) & 0x3)
    {         // Битове 9:8
    case 0x0: // ADD Rd, Rm
        result = value1 + value2;
        if (rd_idx == 15)
        {                   // Ако Rd е PC
            result &= ~0x1; // Изчистване на Thumb бит
        }
        // update_flags();
        break;
    case 0x1: // CMP Rn, Rm
        result = value1 - value2;
        // update_flags();
        return 0;
    case 0x2: // MOV Rd, Rm
        result = value2;
        if (rd_idx == 15)
        {                   // Ако Rd е PC
            result &= ~0x1; // Изчистване на Thumb бит
        }
        // update_flags();
        break;
    default:
        return -1;
    }

    CPU.REG.r[rd_idx] = result;
    return 0;
}

// STR Rd, [Rn, Rm], STRH, STRB, LDRSB, LDR, LDRH, LDRB, LDRSH
int execute_2_str_rd_rd_rm(void)
{
    FUNC_VM();
    uint32_t rm = (CPU.op >> 6) & 0x7;             // Rm
    uint32_t rn = (CPU.op >> 3) & 0x7;             // Rn
    uint32_t rd = CPU.op & 0x7;                    // Rd
    uint32_t addr = CPU.REG.r[rn] + CPU.REG.r[rm]; // Адрес = Rn + Rm
    int res;

    switch ((CPU.op >> 9) & 0xF)
    {         // Битове 12:9
    case 0x8: // STR Rd, [Rn, Rm]
        return WRITE_MEM_32(addr, CPU.REG.r[rd]);
    case 0x9: // STRH Rd, [Rn, Rm]
        return WRITE_MEM_16(addr, CPU.REG.r[rd] & 0xFFFF);
    case 0xA: // STRB Rd, [Rn, Rm]
        return WRITE_MEM_8(addr, CPU.REG.r[rd] & 0xFF);
    case 0xB: // LDRSB Rd, [Rn, Rm]
        CPU.REG.r[rd] = (int32_t)(int8_t)READ_MEM_8(addr, &res);
        return res;
    case 0xC: // LDR Rd, [Rn, Rm]
        CPU.REG.r[rd] = READ_MEM_32(addr, &res);
        return res;
    case 0xD: // LDRH Rd, [Rn, Rm]
        CPU.REG.r[rd] = READ_MEM_16(addr, &res);
        return res;
    case 0xE: // LDRB Rd, [Rn, Rm]
        CPU.REG.r[rd] = READ_MEM_8(addr, &res);
        return res;
    case 0xF: // LDRSH Rd, [Rn, Rm]
        CPU.REG.r[rd] = (int32_t)(int16_t)READ_MEM_16(addr, &res);
        return res;
    default:
        return -1;
    }
}

// LDR Rd, [PC, #]
int execute_2_ldr_pc(void)
{
    FUNC_VM();
    uint32_t rd = (CPU.op >> 8) & 0x7;     // Rd (R0–R7)
    uint32_t imm8 = CPU.op & 0xFF;         // imm8 (0–255)
    uint32_t pc = (CPU.REG.PC + 4) & ~0x3; // Подравнен PC + 4 (pipeline offset)
    uint32_t addr = pc + (imm8 << 2);      // Адрес = Align(PC + 4, 4) + imm8*4
    int res;
    uint32_t value = READ_MEM_32(addr, &res); // Четене от паметта
    if (res)
    {
        DEBUG_M4("[ERROR] Memory read failed at address 0x%08X\n", addr);
        return res;
    }
    CPU.REG.r[rd] = value; // Запис в Rd
    return 0;
}

static int execute_2(void)
{
    FUNC_VM();
    uint32_t op = CPU.op & 0x1FFF; // Премахване на битове 15:13

    // >>6 за AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR, TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN
    switch (op >> 6)
    {
    case 0x0: // AND
    case 0x1: // EOR
    case 0x2: // LSL
    case 0x3: // LSR
    case 0x4: // ASR
    case 0x5: // ADC
    case 0x6: // SBC
    case 0x7: // ROR
    case 0x8: // TST
    case 0x9: // NEG
    case 0xA: // CMP
    case 0xB: // CMN
    case 0xC: // ORR
    case 0xD: // MUL
    case 0xE: // BIC
    case 0xF: // MVN
        return execute_2_and_rd_rm();
    }

    // >>7 за BX, BLX
    switch (op >> 7)
    {
    case 0x1C: // BX
    case 0x1F: // BLX
        return execute_2_bx_rm();
    }

    // >>8 за ADD, CMP, MOV
    switch (op >> 8)
    {
    case 0x4: // ADD
    case 0x5: // CMP
    case 0x6: // MOV
        return execute_2_add_rd_rm();
    }

    // >>9 за STR, STRH, STRB, LDRSB, LDR, LDRH, LDRB, LDRSH
    switch (op >> 9)
    {
    case 0x8: // STR
    case 0x9: // STRH
    case 0xA: // STRB
    case 0xB: // LDRSB
    case 0xC: // LDR
    case 0xD: // LDRH
    case 0xE: // LDRB
    case 0xF: // LDRSH
        return execute_2_str_rd_rd_rm();
    }

    // >>10 за LDR Rd, [PC, #]
    switch (op >> 11)
    {
    case 0x1: // 01001xxx
        return execute_2_ldr_pc();
    }

    DEBUG_M4("[ERROR] Unknown Group 2 Instruction: 0x%04X\n", CPU.op);
    return -1;
}

// GROUP 3 ////////////////////////////

/*
    STR  Rd, [Rn, #OFF]     [011 00 # Offset Rn Rd]
    LDR  Rd, [Rn, #OFF]     [011 01 # Offset Rn Rd]
    STRB Rd, [Rn, #OFF]     [011 10 # Offset Rn Rd]
    LDRB Rd, [Rn, #OFF]     [011 11 # Offset Rn Rd]
*/

static int execute_3(void)
{
    FUNC_VM();
    uint32_t op = CPU.op & 0x1FFF;        // Премахване на битове 15:13
    uint32_t imm5 = (CPU.op >> 6) & 0x1F; // Offset (битове 10:6)
    uint32_t rn = (CPU.op >> 3) & 0x7;    // Rn (битове 5:3)
    uint32_t rd = CPU.op & 0x7;           // Rd (битове 2:0)
    uint32_t address;
    int res;
    switch (op >> 11) // (битове 12:11)
    {
    case 0:
        PRINTF("\tSTR Rd, [Rn, #OFF]\n");
        address = CPU.REG.r[rn] + (imm5 << 2); // Offset = imm5 * 4
        return WRITE_MEM_32(address, CPU.REG.r[rd]);
    case 1:
        PRINTF("\tLDR Rd, [Rn, #OFF]\n");
        address = CPU.REG.r[rn] + (imm5 << 2); // Offset = imm5 * 4
        CPU.REG.r[rd] = READ_MEM_32(address, &res);
        return res;
    case 2:
        PRINTF("\tSTRB Rd, [Rn, #OFF]\n");
        address = CPU.REG.r[rn] + imm5; // Offset = imm5
        return WRITE_MEM_8(address, CPU.REG.r[rd] & 0xFF);
    case 3:
        PRINTF("\tLDRB Rd, [Rn, #OFF]\n");
        address = CPU.REG.r[rn] + imm5; // Offset = imm5
        CPU.REG.r[rd] = READ_MEM_8(address, &res);
        return res;
    default:
        DEBUG_M4("[ERROR] Unknown Group 3 Instruction: 0x%04X\n", CPU.op);
        return -1;
    }
}

// GROUP 4 ////////////////////////////
/*
    STRH Rd, [Rn, #OFF]     [100 00 # Offset Rn Rd]
    LDRH Rd, [Rn, #OFF]     [100 01 # Offset Rn Rd]
    STR Rd,  [SP, #OFF]     [100 10 Rd SP Relative Offset]
    LDR Rd,  [SP, #OFF]     [100 11 Rd SP Relative Offset]
*/

static int execute_4(void)
{
    FUNC_VM();
    uint32_t op = CPU.op & 0x1FFF; // Премахване на битове 15:13
    uint32_t rd, address;
    int res;

    switch (op >> 11)
    {       // Битове 12:11
    case 0: // STRH Rd, [Rn, #OFF]
    {
        uint32_t imm5 = (CPU.op >> 6) & 0x1F;  // Offset (битове 10:6)
        uint32_t rn = (CPU.op >> 3) & 0x7;     // Rn (битове 5:3)
        rd = CPU.op & 0x7;                     // Rd (битове 2:0)
        address = CPU.REG.r[rn] + (imm5 << 1); // Offset = imm5 * 2
        return WRITE_MEM_16(address, CPU.REG.r[rd] & 0xFFFF);
    }
    case 1: // LDRH Rd, [Rn, #OFF]
    {
        uint32_t imm5 = (CPU.op >> 6) & 0x1F;  // Offset (битове 10:6)
        uint32_t rn = (CPU.op >> 3) & 0x7;     // Rn (битове 5:3)
        rd = CPU.op & 0x7;                     // Rd (битове 2:0)
        address = CPU.REG.r[rn] + (imm5 << 1); // Offset = imm5 * 2
        CPU.REG.r[rd] = READ_MEM_16(address, &res);
        return res;
    }
    case 2: // STR Rd, [SP, #OFF]
    {
        rd = (CPU.op >> 8) & 0x7;           // Rd (битове 10:8)
        uint32_t imm8 = CPU.op & 0xFF;      // Offset (битове 7:0)
        address = CPU.REG.SP + (imm8 << 2); // Offset = imm8 * 4
        return WRITE_MEM_32(address, CPU.REG.r[rd]);
    }
    case 3: // LDR Rd, [SP, #OFF]
    {
        rd = (CPU.op >> 8) & 0x7;           // Rd (битове 10:8)
        uint32_t imm8 = CPU.op & 0xFF;      // Offset (битове 7:0)
        address = CPU.REG.SP + (imm8 << 2); // Offset = imm8 * 4
        CPU.REG.r[rd] = READ_MEM_32(address, &res);
        return res;
    }
    default:
        DEBUG_M4("[ERROR] Unknown Group 4 Instruction: 0x%04X\n", CPU.op);
        return -1;
    }
}

// GROUP 5 ////////////////////////////

static int execute_5(void)
{
    FUNC_VM();
    int res;
    uint32_t op = (CPU.op >> 8) & 0xFF; // Битове 15:8 за декодиране

    // ADD Rd, PC, #OFF [101 00 Rd imm8]
    if ((op & 0xF8) == 0xA0)
    { // 101 00 xxx
        PRINTF("\tADD Rd, PC, #OFF\n");
        uint32_t rd = (CPU.op >> 8) & 0x7; // Rd (R0–R7)
        uint32_t imm8 = CPU.op & 0xFF;     // imm8
        uint32_t pc = CPU.REG.PC & ~0x3;   // Подравнен PC
        CPU.REG.r[rd] = pc + (imm8 << 2);  // Rd = PC + imm8*4
        return 0;
    }
    // ADD Rd, SP, #OFF [101 01 Rd imm8]
    else if ((op & 0xF8) == 0xA8)
    { // 101 01 xxx
        PRINTF("\tADD Rd, SP, #OFF\n");
        uint32_t rd = (CPU.op >> 8) & 0x7;        // Rd (R0–R7)
        uint32_t imm8 = CPU.op & 0xFF;            // imm8
        CPU.REG.r[rd] = CPU.REG.SP + (imm8 << 2); // Rd = SP + imm8*4
        return 0;
    }
    // SUB SP, SP, #OFF [101 100001 imm7]
    else if (op == 0xB0)
    { // 101 100001
        PRINTF("\tSUB SP, SP, #OFF\n");
        uint32_t imm7 = CPU.op & 0x7F; // imm7 (7 бита)
        CPU.REG.SP -= (imm7 << 2);     // SP = SP - imm7*4
        return 0;
    }
    // PUSH {<reg list>, <LR>} [101 1010 M reglist]
    else if ((op & 0xFE) == 0xB4)
    { // 101 1010 x
        PRINTF("\tPUSH {<reg list>, <LR>}\n");
        uint32_t reglist = CPU.op & 0xFF;                  // R0–R7
        uint32_t lr = (CPU.op >> 8) & 0x1;                 // M (LR)
        uint32_t count = __builtin_popcount(reglist) + lr; // Брой регистри
        uint32_t addr = CPU.REG.SP;
        // Запис в стека (намаляващ стек)
        for (int i = 0; i < 8; i++)
        {
            if (reglist & (1 << i))
            {
                addr -= 4;
                if (WRITE_MEM_32(addr, CPU.REG.r[i])) // Запис на Ri
                    return -1;
            }
        }
        if (lr)
        {
            addr -= 4;
            if (WRITE_MEM_32(addr, CPU.REG.LR)) // Запис на LR
                return -1;
        }
        CPU.REG.SP = addr; // Актуализация на SP
        return 0;
    }
    // POP {<reg list>, <PC>} [101 1110 P reglist]
    else if ((op & 0xFE) == 0xBC)
    { // 101 1110 x
        PRINTF("\tPOP {<reg list>, <PC>}\n");
        uint32_t reglist = CPU.op & 0xFF;  // R0–R7
        uint32_t pc = (CPU.op >> 8) & 0x1; // P (PC)
        uint32_t addr = CPU.REG.SP;
        // Четене от стека
        for (int i = 0; i < 8; i++)
        {
            if (reglist & (1 << i))
            {
                CPU.REG.r[i] = READ_MEM_32(addr, &res); // Четене в Ri
                if (res)
                    return -1;
                addr += 4;
            }
        }
        if (pc)
        {
            CPU.REG.PC = READ_MEM_32(addr, &res) & ~0x1; // Четене в PC, Thumb бит=0
            if (res)
                return -1;
            addr += 4;
        }
        CPU.REG.SP = addr; // Актуализация на SP
        return 0;
    }
    // BKPT # [101 11110 imm8]
    else if (op == 0xBE)
    { // 101 11110
        PRINTF("\tBKPT #\n");
        uint32_t imm8 = CPU.op & 0xFF; // imm8
        // Спиране за дебъгване (зависи от системата)
        // trigger_breakpoint(imm8); // Хипотетична функция
        return 0;
    }

    return -1; // Невалиден опкод
}

// GROUP 6 ////////////////////////////
/*
    STMIA Rn!, {<reg list>}     [110 0 0 Rn Register List]
    LDMIA Rn!, {<reg list>}     [110 0 1 Rn Register List]
    B{<cond>} <Target Addr>     [110 1 cond # Offset]
    //Unused Opcode             [110 1 1 1 1 0 x x x x x x x x] НЕ !
    SWI #                       [110 1 1 1 1 1 #] return 0 !
*/

// Помощна функция за проверка на условията за B{<cond>}
static int check_condition(uint32_t cond)
{
    switch (cond)
    {
    case 0x0:
        return CPU.psr.apsr.Z; // EQ
    case 0x1:
        return !CPU.psr.apsr.Z; // NE
    case 0x2:
        return CPU.psr.apsr.C; // CS/HS
    case 0x3:
        return !CPU.psr.apsr.C; // CC/LO
    case 0x4:
        return CPU.psr.apsr.N; // MI
    case 0x5:
        return !CPU.psr.apsr.N; // PL
    case 0x6:
        return CPU.psr.apsr.V; // VS
    case 0x7:
        return !CPU.psr.apsr.V; // VC
    case 0x8:
        return CPU.psr.apsr.C && !CPU.psr.apsr.Z; // HI
    case 0x9:
        return !CPU.psr.apsr.C || CPU.psr.apsr.Z; // LS
    case 0xA:
        return CPU.psr.apsr.N == CPU.psr.apsr.V; // GE
    case 0xB:
        return CPU.psr.apsr.N != CPU.psr.apsr.V; // LT
    case 0xC:
        return !CPU.psr.apsr.Z && (CPU.psr.apsr.N == CPU.psr.apsr.V); // GT
    case 0xD:
        return CPU.psr.apsr.Z || (CPU.psr.apsr.N != CPU.psr.apsr.V); // LE
    case 0xE:
        return 1; // AL (винаги изпълнява)
    default:
        return 0; // Невалидно условие
    }
}

static int execute_6(void)
{
    FUNC_VM();
    uint32_t op = CPU.op & 0x1FFF; // Премахване на битове 15:13

    // Проверка на бит 12
    if ((op >> 12) == 0)
    {                                     // STMIA или LDMIA
        uint32_t rn = (op >> 8) & 0x7;    // Rn (битове 10:8)
        uint32_t reg_list = op & 0xFF;    // Register List (битове 7:0)
        uint32_t address = CPU.REG.r[rn]; // Начален адрес
        int res;

        if (reg_list == 0)
        { // Празен списък е невалиден
            DEBUG_M4("[ERROR] Empty register list in STMIA/LDMIA: 0x%04X\n", CPU.op);
            return -1;
        }

        if ((op >> 11) & 0x1)
        { // LDMIA Rn!, {<reg list>}
            for (int i = 0; i < 8; i++)
            {
                if (reg_list & (1 << i))
                {
                    CPU.REG.r[i] = READ_MEM_32(address, &res);
                    if (res)
                    {
                        DEBUG_M4("[ERROR] Memory read failed at 0x%08X\n", address);
                        return res;
                    }
                    address += 4;
                }
            }
        }
        else
        { // STMIA Rn!, {<reg list>}
            for (int i = 0; i < 8; i++)
            {
                if (reg_list & (1 << i))
                {
                    res = WRITE_MEM_32(address, CPU.REG.r[i]);
                    if (res)
                    {
                        DEBUG_M4("[ERROR] Memory write failed at 0x%08X\n", address);
                        return res;
                    }
                    address += 4;
                }
            }
        }

        CPU.REG.r[rn] = address; // Write-back на Rn
        return 0;
    }
    else
    { // B{<cond>} или SWI
        if (((op >> 6) & 0x3F) == 0x3F)
        { // SWI #

            uint32_t imm6 = op & 0x3F; // Immediate (битове 5:0)
            // TODO: Извикване на обработчик за SWI (зависи от системата)
            DEBUG_M4("[INFO] SWI %d executed\n", imm6);
            return 0; // Според спецификацията връща 0
        }
        else if (((op >> 8) & 0xF) == 0xE)
        { // Unused Opcode [110 1 1 1 1 0 ...]
            DEBUG_M4("[ERROR] Unused opcode: 0x%04X\n", CPU.op);
            return -1;
        }
        else
        { // B{<cond>} <Target Addr>

            uint32_t cond = (op >> 8) & 0xF; // Условие (битове 11:8)
            int8_t offset = op & 0xFF;       // Знаков offset (битове 7:0)

            if (!check_condition(cond))
            {
                return 0; // Условието не е изпълнено, не правим скок
            }

            // Изчисляване на целевия адрес: PC + 4 + (offset * 2)
            uint32_t target = (CPU.REG.PC + 4) + ((int32_t)offset << 1);
            CPU.REG.PC = target & ~0x1; // Подравняване и запазване на Thumb бит
            return 0;
        }
    }

    DEBUG_M4("[ERROR] Unknown Group 6 Instruction: 0x%04X\n", CPU.op);
    return -1;
}

// GROUP 7 ////////////////////////////

/*
    B <Target Addr>             [111 00 # Offset]
    BLX <Target Addr>           [111 01 # Offset (lower half)]
    BL{X} <Target Addr> (+)     [111 10 # Offset (upper half)]
    BL <Target Addr>            [111 11 # Offset (lower half)]
*/

// GROUP 7 ////////////////////////////
/*
    B <Target Addr>             [111 00 # Offset]
    BLX <Target Addr>           [111 01 # Offset (lower half)]
    BL{X} <Target Addr> (+)     [111 10 # Offset (upper half)]
    BL <Target Addr>            [111 11 # Offset (lower half)]
*/

// Статични променливи за BL/BLX състояние
static uint32_t upper_offset = 0; // Горна половина на офсета
static int is_upper_pending = 0;  // Флаг за чакаща горна половина

static int execute_7(void)
{
    FUNC_VM();
    uint32_t op = CPU.op & 0x1FFF; // Премахване на битове 15:13

    switch (op >> 11) // Битове 12:11
    {
    case 0: // B <Target Addr>
    {
        int32_t offset = ((int32_t)(op & 0x7FF) << 21) >> 20; // Знаков 11-битов офсет
        uint32_t target = (CPU.REG.PC + 4) + (offset << 1);   // PC + 4 + offset*2
        CPU.REG.PC = target & ~0x1;                           // Подравняване за Thumb
        is_upper_pending = 0;                                 // Изчистване на BL/BLX състояние
        return 0;
    }
    case 2: // BL{X} <Target Addr> (upper half)
    {
        upper_offset = (op & 0x7FF) << 11; // Съхраняване на горните 11 бита
        is_upper_pending = 1;              // Отбелязваме, че чакаме долна половина
        return 0;
    }
    case 1: // BLX <Target Addr> (lower half)
    case 3: // BL <Target Addr> (lower half)
    {
        if (!is_upper_pending)
        {
            DEBUG_M4("[ERROR] BL/BLX lower half without upper half: 0x%04X\n", CPU.op);
            return -1;
        }

        uint32_t lower_offset = (op & 0x7FF) << 1;    // Долните 11 бита, изместени с 1
        int32_t offset = upper_offset | lower_offset; // Комбиниран 22-битов офсет
        if ((op >> 11) == 1)
        {                         // BLX: Добавяме H бит за подравняване
            offset |= (op & 0x1); // H бит (bit 0)
        }
        else
        {                                             // BL: Знаково разширение
            offset = ((int32_t)(offset << 10) >> 10); // Разширяване на знака
        }

        uint32_t target = (CPU.REG.PC + 4) + offset; // Целеви адрес
        CPU.REG.LR = (CPU.REG.PC + 2) | 0x1;         // Запазване на следващия адрес (Thumb)
        CPU.REG.PC = target & ~0x1;                  // Подравняване за Thumb
        is_upper_pending = 0;                        // Изчистване на състояние
        return 0;
    }
    default:
        DEBUG_M4("[ERROR] Unknown Group 7 Instruction: 0x%04X\n", CPU.op);
        is_upper_pending = 0; // Изчистване на състояние при грешка
        return -1;
    }
}

// EXECUTE 16 bytes  //////////////////

int m4_execute_16(void)
{
    FUNC_VM();
    int res;

    int op = CPU.op >> 13;
    DEBUG_M4("[V] INSTRUCTION [16]: 0x%04X, OP: %d\n", CPU.op, op);

    switch (op)
    {
    case 0b000: // 0
        res = execute_0();
        break;
    case 0b001: // 1
        res = execute_1();
        break;
    case 0b010: // 2
        res = execute_2();
        break;
    case 0b011: // 3
        res = execute_3();
        break;
    case 0b100: // 4
        res = execute_4();
        break;
    case 0b101: // 5
        res = execute_5();
        break;
    case 0b110: // 6
        res = execute_6();
        break;
    case 0b111: // 7
        res = execute_7();
        break;
    default:
        DEBUG_M4("[ERROR] Unknown Instruction: 0x%04X, PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        res = -1;
        break;
    }

    if (res)
    {
        //PRINT_REG(); // отпечатва регистри
    }
    else
    {
        //PRINT_REG(); // отпечатва регистри
        CPU.REG.PC += 2;
    }

    RETURN_ERROR(res); // OK = 0 / ERROR = -1
}

///////////////////////////////////////