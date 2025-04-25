#include "M4.h"

// Функция за Shift (LSL, LSR, ASR), ADD/SUB register/immediate
static int execute_shift_add_sub(uint16_t op)
{
    uint32_t result;

    if ((op & 0x1800) == 0x0000) // Shift
    {
        uint32_t imm5 = (op >> 6) & 0x1F;
        uint32_t Rm = (op >> 3) & 0x7;
        uint32_t Rd = op & 0x7;
        uint32_t shift_type = (op >> 11) & 0x3;

        switch (shift_type)
        {
        case 0b00: // LSL
            result = CPU.REG.r[Rm] << imm5;
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_LSL, imm5, UPDATE_N | UPDATE_Z | UPDATE_C);
            break;
        case 0b01: // LSR
            if (imm5 == 0)
            {
                result = 0;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_LSR, 32, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            else
            {
                result = CPU.REG.r[Rm] >> imm5;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_LSR, imm5, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            break;
        case 0b10: // ASR
            if (imm5 == 0)
            {
                result = ((int32_t)CPU.REG.r[Rm]) < 0 ? 0xFFFFFFFF : 0;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_ASR, 32, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            else
            {
                result = ((int32_t)CPU.REG.r[Rm]) >> imm5;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_ASR, imm5, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            break;
        default:
            M4_DEBUG("[ERROR] Invalid shift type: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else if ((op & 0x1C00) == 0x1800) // ADD/SUB register
    {
        uint32_t Rm = (op >> 6) & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t Rd = op & 0x7;
        uint32_t sub = (op >> 9) & 0x1;

        if (sub) // SUB
        {
            result = CPU.REG.r[Rn] - CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_SUB, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
        }
        else // ADD
        {
            result = CPU.REG.r[Rn] + CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_ADD, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
        }
    }
    else if ((op & 0x1C00) == 0x1C00) // ADD/SUB immediate
    {
        uint32_t imm3 = (op >> 6) & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t Rd = op & 0x7;
        uint32_t sub = (op >> 9) & 0x1;

        if (sub) // SUB
        {
            result = CPU.REG.r[Rn] - imm3;
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], imm3, OP_SUB, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
        }
        else // ADD
        {
            result = CPU.REG.r[Rn] + imm3;
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], imm3, OP_ADD, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
        }
    }
    else
    {
        M4_DEBUG("[ERROR] Unknown shift/add/sub op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    CPU.REG.PC += 2;
    return 0;
}

// Функция за MOV immediate, CMP immediate, ADD/SUB immediate
static int execute_mov_cmp_add_sub(uint16_t op)
{
    uint32_t Rd = (op >> 8) & 0x7;
    uint32_t imm8 = op & 0xFF;
    uint32_t op2 = (op >> 11) & 0x3;
    uint32_t result;

    switch (op2)
    {
    case 0b00: // MOV immediate
        result = imm8;
        CPU.REG.r[Rd] = result;
        m4_update_apsr(result, result, 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
        break;
    case 0b01: // CMP immediate
        result = CPU.REG.r[Rd] - imm8;
        m4_update_apsr(result, CPU.REG.r[Rd], imm8, OP_CMP, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
        break;
    case 0b10: // ADD immediate
        result = CPU.REG.r[Rd] + imm8;
        CPU.REG.r[Rd] = result;
        m4_update_apsr(result, CPU.REG.r[Rd], imm8, OP_ADD, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
        break;
    case 0b11: // SUB immediate
        result = CPU.REG.r[Rd] - imm8;
        CPU.REG.r[Rd] = result;
        m4_update_apsr(result, CPU.REG.r[Rd], imm8, OP_SUB, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
        break;
    default:
        M4_DEBUG("[ERROR] Unknown op2 in MOV/CMP/ADD/SUB: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    CPU.REG.PC += 2;
    return 0;
}

// Функция за Data Processing, Load/Store Single, PC-relative Load
static int execute_data_processing_load_store(uint16_t op)
{
    uint32_t result;

    if ((op & 0x1000) == 0x0000) // Data Processing
    {
        uint32_t Rm = (op >> 6) & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t Rd = op & 0x7;
        uint32_t opcode = (op >> 9) & 0xF;

        switch (opcode)
        {
        case 0b0000: // AND
            result = CPU.REG.r[Rn] & CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_AND, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b0001: // EOR
            result = CPU.REG.r[Rn] ^ CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_EOR, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b0010: // LSL
        {
            uint32_t shift_amount = CPU.REG.r[Rm] & 0xFF;
            if (shift_amount == 0)
            {
                result = CPU.REG.r[Rn];
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_LSL, 0, UPDATE_N | UPDATE_Z);
            }
            else if (shift_amount >= 32)
            {
                result = 0;
                uint32_t carry = (shift_amount == 32) ? ((CPU.REG.r[Rn] >> 31) & 1) : 0;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], carry, OP_LSL, shift_amount, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            else
            {
                result = CPU.REG.r[Rn] << shift_amount;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_LSL, shift_amount, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            break;
        }
        case 0b0011: // LSR
        {
            uint32_t shift_amount = CPU.REG.r[Rm] & 0xFF;
            if (shift_amount == 0)
            {
                result = CPU.REG.r[Rn];
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_LSR, 0, UPDATE_N | UPDATE_Z);
            }
            else if (shift_amount >= 32)
            {
                result = 0;
                uint32_t carry = (shift_amount == 32) ? ((CPU.REG.r[Rn] >> 31) & 1) : 0;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], carry, OP_LSR, shift_amount, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            else
            {
                result = CPU.REG.r[Rn] >> shift_amount;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_LSR, shift_amount, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            break;
        }
        case 0b0100: // ASR
        {
            uint32_t shift_amount = CPU.REG.r[Rm] & 0xFF;
            if (shift_amount == 0)
            {
                result = CPU.REG.r[Rn];
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_ASR, 0, UPDATE_N | UPDATE_Z);
            }
            else if (shift_amount >= 32)
            {
                result = ((int32_t)CPU.REG.r[Rn]) < 0 ? 0xFFFFFFFF : 0;
                uint32_t carry = (shift_amount == 32) ? ((CPU.REG.r[Rn] >> 31) & 1) : (((int32_t)CPU.REG.r[Rn]) < 0);
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], carry, OP_ASR, shift_amount, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            else
            {
                result = ((int32_t)CPU.REG.r[Rn]) >> shift_amount;
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_ASR, shift_amount, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            break;
        }
        case 0b0101: // ADC
            result = CPU.REG.r[Rn] + CPU.REG.r[Rm] + CPU.psr.apsr.C;
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_ADC, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b0110: // SBC
            result = CPU.REG.r[Rn] - CPU.REG.r[Rm] - !CPU.psr.apsr.C;
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_SBC, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b0111: // ROR
        {
            uint32_t shift_amount = CPU.REG.r[Rm] & 0xFF;
            if (shift_amount == 0)
            {
                result = CPU.REG.r[Rn];
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_ROR, 0, UPDATE_N | UPDATE_Z);
            }
            else
            {
                shift_amount = shift_amount % 32; // ROR се нормализира mod 32
                result = (CPU.REG.r[Rn] >> shift_amount) | (CPU.REG.r[Rn] << (32 - shift_amount));
                CPU.REG.r[Rd] = result;
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_ROR, shift_amount, UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            break;
        }
        case 0b1000: // TST
            result = CPU.REG.r[Rn] & CPU.REG.r[Rm];
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_TST, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b1001: // RSB
            result = -CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, 0, CPU.REG.r[Rm], OP_RSB, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b1010: // CMP
            result = CPU.REG.r[Rn] - CPU.REG.r[Rm];
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_CMP, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b1011: // CMN
            result = CPU.REG.r[Rn] + CPU.REG.r[Rm];
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_CMN, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b1100: // ORR
            result = CPU.REG.r[Rn] | CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_ORR, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b1101: // MUL
            result = CPU.REG.r[Rn] * CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_MUL, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b1110: // BIC
            result = CPU.REG.r[Rn] & ~CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_BIC, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b1111: // MVN
            result = ~CPU.REG.r[Rm];
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, 0, CPU.REG.r[Rm], OP_MVN, 0, UPDATE_N | UPDATE_Z);
            break;
        default:
            M4_DEBUG("[ERROR] Unknown data processing opcode: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else if ((op & 0xFC00) == 0xB200) // SXTH, SXTB, UXTH, UXTB
    {
        uint32_t Rm = (op >> 3) & 0x7;
        uint32_t Rd = op & 0x7;
        uint32_t opcode = (op >> 6) & 0x3;

        switch (opcode)
        {
        case 0b00: // SXTH
            result = (int16_t)(CPU.REG.r[Rm] & 0xFFFF);
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b01: // SXTB
            result = (int8_t)(CPU.REG.r[Rm] & 0xFF);
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10: // UXTH
            result = CPU.REG.r[Rm] & 0xFFFF;
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b11: // UXTB
            result = CPU.REG.r[Rm] & 0xFF;
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        }
    }
    else if ((op & 0xFC00) == 0xBA00) // REV, REV16, REVSH
    {
        uint32_t Rm = (op >> 3) & 0x7;
        uint32_t Rd = op & 0x7;
        uint32_t opcode = (op >> 6) & 0x3;

        switch (opcode)
        {
        case 0b00: // REV
            result = ((CPU.REG.r[Rm] & 0xFF) << 24) |
                     ((CPU.REG.r[Rm] & 0xFF00) << 8) |
                     ((CPU.REG.r[Rm] & 0xFF0000) >> 8) |
                     ((CPU.REG.r[Rm] & 0xFF000000) >> 24);
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b01: // REV16
            result = ((CPU.REG.r[Rm] & 0xFF) << 8) |
                     ((CPU.REG.r[Rm] & 0xFF00) >> 8) |
                     ((CPU.REG.r[Rm] & 0xFF0000) << 8) |
                     ((CPU.REG.r[Rm] & 0xFF000000) >> 8);
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10: // REVSH
            result = (int16_t)(((CPU.REG.r[Rm] & 0xFF) << 8) |
                               ((CPU.REG.r[Rm] & 0xFF00) >> 8));
            CPU.REG.r[Rd] = result;
            m4_update_apsr(result, CPU.REG.r[Rm], 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        default:
            M4_DEBUG("[ERROR] Unknown reverse op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else if ((op & 0xF800) == 0x4800) // LDR PC-relative
    {
        uint32_t Rd = (op >> 8) & 0x7;
        uint32_t imm8 = op & 0xFF;
        uint32_t address = (CPU.REG.PC & ~0x3) + (imm8 << 2);

        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for LDR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        if (address < ROM_BASE || address >= ROM_BASE + CPU.ROM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid ROM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        CPU.REG.r[Rd] = READ_ROM_32(address);
    }
    else if ((op & 0xF800) == 0x6000) // STR immediate
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t imm5 = (op >> 6) & 0x1F;
        uint32_t address = CPU.REG.r[Rn] + (imm5 << 2);

        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for STR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        WRITE_RAM_32(address, CPU.REG.r[Rt]);
    }
    else if ((op & 0xF800) == 0x6800) // LDR immediate
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t imm5 = (op >> 6) & 0x1F;
        uint32_t address = CPU.REG.r[Rn] + (imm5 << 2);

        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for LDR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        CPU.REG.r[Rt] = READ_RAM_32(address);
    }
    else if ((op & 0xF800) == 0x7000) // STRB immediate
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t imm5 = (op >> 6) & 0x1F;
        uint32_t address = CPU.REG.r[Rn] + imm5;

        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        WRITE_RAM_8(address, CPU.REG.r[Rt] & 0xFF);
    }
    else if ((op & 0xF800) == 0x7800) // LDRB immediate
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t imm5 = (op >> 6) & 0x1F;
        uint32_t address = CPU.REG.r[Rn] + imm5;

        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        CPU.REG.r[Rt] = READ_RAM_8(address);
    }
    else if ((op & 0xF800) == 0x8000) // STRH immediate
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t imm5 = (op >> 6) & 0x1F;
        uint32_t address = CPU.REG.r[Rn] + (imm5 << 1);

        if (address & 1) // 2-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for STRH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        WRITE_RAM_16(address, CPU.REG.r[Rt] & 0xFFFF);
    }
    else if ((op & 0xF800) == 0x8800) // LDRH immediate
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t imm5 = (op >> 6) & 0x1F;
        uint32_t address = CPU.REG.r[Rn] + (imm5 << 1);

        if (address & 1) // 2-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for LDRH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        CPU.REG.r[Rt] = READ_RAM_16(address);
    }
    else if ((op & 0xF800) == 0x9000) // STR SP-relative
    {
        uint32_t Rt = (op >> 8) & 0x7;
        uint32_t imm8 = op & 0xFF;
        uint32_t address = CPU.REG.SP + (imm8 << 2);

        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for STR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        WRITE_RAM_32(address, CPU.REG.r[Rt]);
    }
    else if ((op & 0xF800) == 0x9800) // LDR SP-relative
    {
        uint32_t Rt = (op >> 8) & 0x7;
        uint32_t imm8 = op & 0xFF;
        uint32_t address = CPU.REG.SP + (imm8 << 2);

        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for LDR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        CPU.REG.r[Rt] = READ_RAM_32(address);
    }
    else
    {
        M4_DEBUG("[ERROR] Unknown data processing/load/store op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    CPU.REG.PC += 2;
    return 0;
}

// Функция за  ???
static int execute_load_store_register(uint16_t op)
{
    if ((op & 0xFE00) == 0x5000) // STR/STRB/STRH register
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t Rm = (op >> 6) & 0x7;
        uint32_t op2 = (op >> 9) & 0x7;
        uint32_t address = CPU.REG.r[Rn] + CPU.REG.r[Rm];

        switch (op2)
        {
        case 0b000: // STR
            if (address & 3)
            {
                M4_DEBUG("[ERROR] Unaligned address for STR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            WRITE_RAM_32(address, CPU.REG.r[Rt]);
            break;
        case 0b001: // STRH
            if (address & 1)
            {
                M4_DEBUG("[ERROR] Unaligned address for STRH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            WRITE_RAM_16(address, CPU.REG.r[Rt] & 0xFFFF);
            break;
        case 0b010: // STRB
            if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            WRITE_RAM_8(address, CPU.REG.r[Rt] & 0xFF);
            break;
        default:
            M4_DEBUG("[ERROR] Unknown store register op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else if ((op & 0xFE00) == 0x5800) // LDR/LDRB/LDRH register
    {
        uint32_t Rt = op & 0x7;
        uint32_t Rn = (op >> 3) & 0x7;
        uint32_t Rm = (op >> 6) & 0x7;
        uint32_t op2 = (op >> 9) & 0x7;
        uint32_t address = CPU.REG.r[Rn] + CPU.REG.r[Rm];

        switch (op2)
        {
        case 0b000: // LDR
            if (address & 3)
            {
                M4_DEBUG("[ERROR] Unaligned address for LDR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            CPU.REG.r[Rt] = READ_RAM_32(address);
            break;
        case 0b001: // LDRH
            if (address & 1)
            {
                M4_DEBUG("[ERROR] Unaligned address for LDRH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            CPU.REG.r[Rt] = READ_RAM_16(address);
            break;
        case 0b010: // LDRB
            if (address < RAM_BASE || address >= RAM_BASE + CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            CPU.REG.r[Rt] = READ_RAM_8(address);
            break;
        default:
            M4_DEBUG("[ERROR] Unknown load register op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else
    {
        M4_DEBUG("[ERROR] Unknown load/store register op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    CPU.REG.PC += 2;
    return 0;
}

// Функция за Load/Store Register Offset
static int execute_load_store_multiple(uint16_t op)
{
    if ((op & 0xF800) == 0xC000) // LDMIA
    {
        uint32_t Rn = (op >> 8) & 0x7;
        uint32_t reg_list = op & 0xFF;
        uint32_t address = CPU.REG.r[Rn];
        int count = 0, i;

        // Брои регистрите в списъка
        for (i = 0; i < 8; i++)
            if (reg_list & (1 << i))
                count++;

        // Проверка за празен списък
        if (count == 0)
        {
            M4_DEBUG("[ERROR] Empty register list for LDMIA: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за Rn в reg_list
        if (reg_list & (1 << Rn))
        {
            M4_DEBUG("[ERROR] Rn in register list for LDMIA: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за подравняване
        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for LDMIA: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за RAM граници
        if (address < RAM_BASE || address + count * 4 > RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Зареждане на регистрите
        for (i = 0; i < 8; i++)
        {
            if (reg_list & (1 << i))
            {
                CPU.REG.r[i] = READ_RAM_32(address);
                address += 4;
            }
        }
        CPU.REG.r[Rn] = address; // Write-back
    }
    else if ((op & 0xF800) == 0xC800) // STMIA
    {
        uint32_t Rn = (op >> 8) & 0x7;
        uint32_t reg_list = op & 0xFF;
        uint32_t address = CPU.REG.r[Rn];
        int count = 0, i;

        // Брои регистрите в списъка
        for (i = 0; i < 8; i++)
            if (reg_list & (1 << i))
                count++;

        // Проверка за празен списък
        if (count == 0)
        {
            M4_DEBUG("[ERROR] Empty register list for STMIA: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за Rn в reg_list
        if (reg_list & (1 << Rn))
        {
            M4_DEBUG("[ERROR] Rn in register list for STMIA: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за подравняване
        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for STMIA: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за RAM граници
        if (address < RAM_BASE || address + count * 4 > RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Запис на регистрите
        for (i = 0; i < 8; i++)
        {
            if (reg_list & (1 << i))
            {
                WRITE_RAM_32(address, CPU.REG.r[i]);
                address += 4;
            }
        }
        CPU.REG.r[Rn] = address; // Write-back
    }
    else if ((op & 0xFE00) == 0xB400) // PUSH
    {
        uint32_t reg_list = op & 0xFF;
        uint32_t P = (op >> 8) & 0x1; // LR bit
        uint32_t address = CPU.REG.SP;
        int count = 0, i;

        // Брои регистрите в списъка
        for (i = 0; i < 8; i++)
            if (reg_list & (1 << i))
                count++;
        if (P)
            count++;

        // Проверка за празен списък
        if (count == 0)
        {
            M4_DEBUG("[ERROR] Empty register list for PUSH: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Изчисляване на новия SP
        address -= count * 4;

        // Проверка за подравняване
        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for PUSH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за RAM граници
        if (address < RAM_BASE || address + count * 4 > RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Запис на регистрите
        uint32_t temp_address = address;
        for (i = 0; i < 8; i++)
        {
            if (reg_list & (1 << i))
            {
                WRITE_RAM_32(temp_address, CPU.REG.r[i]);
                temp_address += 4;
            }
        }
        if (P)
            WRITE_RAM_32(temp_address, CPU.REG.LR);

        CPU.REG.SP = address;
    }
    else if ((op & 0xFE00) == 0xBC00) // POP
    {
        uint32_t reg_list = op & 0xFF;
        uint32_t P = (op >> 8) & 0x1; // PC bit
        uint32_t address = CPU.REG.SP;
        int count = 0, i;

        // Брои регистрите в списъка
        for (i = 0; i < 8; i++)
            if (reg_list & (1 << i))
                count++;
        if (P)
            count++;

        // Проверка за празен списък
        if (count == 0)
        {
            M4_DEBUG("[ERROR] Empty register list for POP: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за подравняване
        if (address & 3) // 4-byte alignment
        {
            M4_DEBUG("[ERROR] Unaligned address for POP: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Проверка за RAM граници
        if (address < RAM_BASE || address + count * 4 > RAM_BASE + CPU.RAM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }

        // Зареждане на регистрите
        for (i = 0; i < 8; i++)
        {
            if (reg_list & (1 << i))
            {
                CPU.REG.r[i] = READ_RAM_32(address);
                address += 4;
            }
        }
        if (P)
        {
            CPU.REG.PC = READ_RAM_32(address) & ~0x1; // Clear Thumb bit
            address += 4;
            CPU.REG.SP = address;
            return 0; // PC е актуализиран, пропускаме CPU.REG.PC += 2
        }
        CPU.REG.SP = address;
    }
    else
    {
        M4_DEBUG("[ERROR] Unknown load/store multiple op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    CPU.REG.PC += 2;
    return 0;
}

// Функция за Branch, ADD/SUB SP, Miscellaneous
static int execute_branch_misc(uint16_t op)
{
    if ((op & 0xF800) == 0xA000) // ADD SP/PC
    {
        uint32_t Rd = (op >> 8) & 0x7;
        uint32_t imm8 = op & 0xFF;
        uint32_t SP = (op >> 11) & 0x1;

        if (SP)
            CPU.REG.r[Rd] = (CPU.REG.SP & ~0x3) + (imm8 << 2); // ADD Rd, SP, #imm
        else
            CPU.REG.r[Rd] = (CPU.REG.PC & ~0x3) + (imm8 << 2); // ADD Rd, PC, #imm
        CPU.REG.PC += 2;
    }
    else if ((op & 0xF800) == 0xA800) // ADD/SUB SP
    {
        uint32_t imm7 = op & 0x7F;
        uint32_t sub = (op >> 7) & 0x1;
        uint32_t new_sp;

        if (sub)
            new_sp = CPU.REG.SP - (imm7 << 2);
        else
            new_sp = CPU.REG.SP + (imm7 << 2);

        if (new_sp & 3) // Проверка за 4-байт подравняване
        {
            M4_DEBUG("[ERROR] Unaligned SP after ADD/SUB SP: 0x%08X at PC: 0x%08X\n", new_sp, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
        CPU.REG.SP = new_sp;
        CPU.REG.PC += 2;
    }
    else if ((op & 0xF000) == 0xB000) // Miscellaneous
    {
        uint32_t op2 = (op >> 8) & 0xF;

        switch (op2)
        {
        case 0x0: // NOP, YIELD, WFE, WFI, SEV
        {
            uint32_t op3 = op & 0xFF;
            switch (op3)
            {
            case 0x00: // NOP
                // Do nothing
                break;
            case 0x01: // YIELD
            case 0x02: // WFE
            case 0x03: // WFI
            case 0x04: // SEV
                M4_DEBUG("[ERROR] System instruction not supported (USE_SYSTEM=0): 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            default:
                M4_DEBUG("[ERROR] Unknown misc instruction: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 2;
                return -1;
            }
            break;
        }
        case 0x1: // CBZ
        case 0x3: // CBNZ
        {
            uint32_t imm5 = (op >> 3) & 0x1F;
            uint32_t i = (op >> 9) & 0x1;
            uint32_t Rn = op & 0x7;
            uint32_t nonzero = (op >> 11) & 0x1;
            uint32_t offset = (i << 6) | (imm5 << 1);

            if ((nonzero && CPU.REG.r[Rn] != 0) || (!nonzero && CPU.REG.r[Rn] == 0))
            {
                CPU.REG.PC += offset + 4;
                if (CPU.REG.PC & 1)
                {
                    M4_DEBUG("[ERROR] Unaligned PC after CBZ/CBNZ: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                    CPU.error = -1;
                    return -1;
                }
                if (CPU.REG.PC < ROM_BASE || CPU.REG.PC >= ROM_BASE + CPU.ROM_SIZE)
                {
                    M4_DEBUG("[ERROR] Invalid ROM access after CBZ/CBNZ: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                    CPU.error = -1;
                    return -1;
                }
            }
            else
            {
                CPU.REG.PC += 2;
            }
            break;
        }
        default:
            M4_DEBUG("[ERROR] Unknown misc op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else if ((op & 0xF000) == 0xD000) // B<cond>
    {
        uint32_t cond = (op >> 8) & 0xF;
        uint32_t imm8 = op & 0xFF;
        int32_t offset = (imm8 << 1);
        if (imm8 & 0x80) // Sign-extend
            offset |= 0xFFFFFF00;

        int branch = 0;
        switch (cond)
        {
        case 0x0:
            branch = CPU.psr.apsr.Z;
            break; // BEQ
        case 0x1:
            branch = !CPU.psr.apsr.Z;
            break; // BNE
        case 0x2:
            branch = CPU.psr.apsr.C;
            break; // BCS
        case 0x3:
            branch = !CPU.psr.apsr.C;
            break; // BCC
        case 0x4:
            branch = CPU.psr.apsr.N;
            break; // BMI
        case 0x5:
            branch = !CPU.psr.apsr.N;
            break; // BPL
        case 0x6:
            branch = CPU.psr.apsr.V;
            break; // BVS
        case 0x7:
            branch = !CPU.psr.apsr.V;
            break; // BVC
        case 0x8:
            branch = CPU.psr.apsr.C && !CPU.psr.apsr.Z;
            break; // BHI
        case 0x9:
            branch = !CPU.psr.apsr.C || CPU.psr.apsr.Z;
            break; // BLS
        case 0xA:
            branch = CPU.psr.apsr.N == CPU.psr.apsr.V;
            break; // BGE
        case 0xB:
            branch = CPU.psr.apsr.N != CPU.psr.apsr.V;
            break; // BLT
        case 0xC:
            branch = !CPU.psr.apsr.Z && (CPU.psr.apsr.N == CPU.psr.apsr.V);
            break; // BGT
        case 0xD:
            branch = CPU.psr.apsr.Z || (CPU.psr.apsr.N != CPU.psr.apsr.V);
            break; // BLE
        default:
            M4_DEBUG("[ERROR] Invalid condition code: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1; // cond=0xE (AL) and 0xF (NV) are not used in Thumb B<cond>
        }

        if (branch)
        {
            CPU.REG.PC += offset + 4;
            if (CPU.REG.PC & 1)
            {
                M4_DEBUG("[ERROR] Unaligned PC after B<cond>: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                CPU.error = -1;
                return -1;
            }
            if (CPU.REG.PC < ROM_BASE || CPU.REG.PC >= ROM_BASE + CPU.ROM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid ROM access after B<cond>: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                CPU.error = -1;
                return -1;
            }
        }
        else
        {
            CPU.REG.PC += 2;
        }
    }
    else if ((op & 0xF800) == 0xE000) // B
    {
        uint32_t imm11 = op & 0x7FF;
        int32_t offset = (imm11 << 1);
        if (imm11 & 0x400) // Sign-extend
            offset |= 0xFFFFF800;

        CPU.REG.PC += offset + 4;
        if (CPU.REG.PC & 1)
        {
            M4_DEBUG("[ERROR] Unaligned PC after B: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
            CPU.error = -1;
            return -1;
        }
        if (CPU.REG.PC < ROM_BASE || CPU.REG.PC >= ROM_BASE + CPU.ROM_SIZE)
        {
            M4_DEBUG("[ERROR] Invalid ROM access after B: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
            CPU.error = -1;
            return -1;
        }
    }
    else
    {
        M4_DEBUG("[ERROR] Unknown branch op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    return 0;
}

// Функция за Branch and Exchange, Branch with Link
static int execute_branch_exchange(uint16_t op)
{
    if ((op & 0xFF80) == 0x4780) // BX
    {
        uint32_t Rm = (op >> 3) & 0xF;
        uint32_t target = CPU.REG.r[Rm];
        if (target & 1) // Thumb mode
        {
            CPU.REG.PC = target & ~0x1;
            if (CPU.REG.PC & 1)
            {
                M4_DEBUG("[ERROR] Unaligned PC after BX: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                CPU.error = -1;
                return -1;
            }
            if (CPU.REG.PC < ROM_BASE || CPU.REG.PC >= ROM_BASE + CPU.ROM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid ROM access after BX: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                CPU.error = -1;
                return -1;
            }
        }
        else
        {
            M4_DEBUG("[ERROR] BX to non-Thumb state: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else if ((op & 0xFF80) == 0x47C0) // BLX
    {
        uint32_t Rm = (op >> 3) & 0xF;
        uint32_t target = CPU.REG.r[Rm];
        if (target & 1) // Thumb mode
        {
            CPU.REG.LR = CPU.REG.PC + 2 | 1;
            CPU.REG.PC = target & ~0x1;
            if (CPU.REG.PC & 1)
            {
                M4_DEBUG("[ERROR] Unaligned PC after BLX: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                CPU.error = -1;
                return -1;
            }
            if (CPU.REG.PC < ROM_BASE || CPU.REG.PC >= ROM_BASE + CPU.ROM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid ROM access after BLX: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 2);
                CPU.error = -1;
                return -1;
            }
        }
        else
        {
            M4_DEBUG("[ERROR] BLX to non-Thumb state: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 2;
            return -1;
        }
    }
    else
    {
        M4_DEBUG("[ERROR] Unknown branch/exchange op: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    return 0;
}

// Функция за Software Interrupt, Undefined
static int execute_swi_undefined(uint16_t op)
{
    if ((op & 0xFF00) == 0xDF00) // SWI
    {
        M4_DEBUG("[ERROR] SWI not supported: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
    else
    {
        M4_DEBUG("[ERROR] Undefined instruction: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
}

// Главна функция за изпълнение на 16-битови инструкции
int m4_execute_16(void)
{
    // Проверка за подравняване на PC
    if (CPU.REG.PC & 0x1)
    {
        M4_DEBUG("[ERROR] Unaligned PC: 0x%08X\n", CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    // Проверка за Thumb състояние
    if (!CPU.psr.epsr.T)
    {
        M4_DEBUG("[ERROR] Invalid Thumb state at PC: 0x%08X\n", CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    // Проверка за граници на ROM
    if (CPU.REG.PC >= CPU.ROM_SIZE + ROM_BASE)
    {
        M4_DEBUG("[ERROR] Invalid PC access: 0x%08X\n", CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    // Четене на 16-битовата инструкция
    CPU.op = READ_ROM_16(CPU.REG.PC);
    uint16_t op = CPU.op;

    // Основно декодиране
    uint8_t op1 = (op >> 13) & 0x7;

    // Извикване на съответната функция според op1
    switch (op1)
    {
    case 0b000:
        return execute_shift_add_sub(op);
    case 0b001:
        return execute_mov_cmp_add_sub(op);
    case 0b010:
        return execute_data_processing_load_store(op);
    case 0b011:
        return execute_load_store_register(op);
    case 0b100:
        return execute_load_store_multiple(op);
    case 0b101:
        return execute_branch_misc(op);
    case 0b110:
        return execute_branch_exchange(op);
    case 0b111:
        return execute_swi_undefined(op);
    default:
        M4_DEBUG("[ERROR] Unknown instruction: 0x%04X at PC: 0x%08X\n", op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 2;
        return -1;
    }
}
