#include "M4.h"

// TODO

int m4_execute_32(void)
{
    // Проверка за граници на ROM
    if (CPU.REG.PC + 3 >= CPU.ROM_SIZE)
    {
        M4_DEBUG("[ERROR] Invalid PC access: 0x%08X\n", CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    // Проверка за DSP инструкции
    if ((CPU.op & 0xFE000000) == 0xFA000000)
    {
        M4_DEBUG("[ERROR] DSP instruction not supported: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 4;
        return -1;
    }

    // Проверка за FPU инструкции
    if ((CPU.op & 0xEF000000) == 0xEE000000 || (CPU.op & 0xEF000000) == 0xEF000000)
    {
        M4_DEBUG("[ERROR] FPU instruction not supported: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 4;
        return -1;
    }

    uint8_t op1 = (CPU.op >> 27) & 0x1F;
    uint32_t result;

    switch (op1)
    {
    case 0b11100: // Branch with Link (BL)
    {
        uint32_t S = (CPU.op >> 26) & 0x1;
        uint32_t imm10 = (CPU.op >> 16) & 0x3FF;
        uint32_t J1 = (CPU.op >> 13) & 0x1;
        uint32_t J2 = (CPU.op >> 11) & 0x1;
        uint32_t imm11 = CPU.op & 0x7FF;
        uint32_t offset = (S << 24) | ((~(J1 ^ S)) << 23) | ((~(J2 ^ S)) << 22) | (imm10 << 12) | (imm11 << 1);
        offset = (offset ^ (1 << 25)) - (1 << 25); // Sign-extend

        CPU.REG.LR = CPU.REG.PC + 4;
        CPU.REG.PC += offset;
        CPU.REG.PC += 4;
        if (CPU.REG.PC & 1)
        {
            M4_DEBUG("[ERROR] Unaligned PC after BL: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 4);
            CPU.error = -1;
            return -1;
        }
        return 0;
    }

    case 0b11101: // Branch (B.W), Table Branch (TBB, TBH)
    {
        uint32_t op2 = (CPU.op >> 20) & 0x7F;
        if (op2 == 0b0000000 || op2 == 0b0000001) // B.W
        {
            uint32_t S = (CPU.op >> 26) & 0x1;
            uint32_t imm10 = (CPU.op >> 16) & 0x3FF;
            uint32_t J1 = (CPU.op >> 13) & 0x1;
            uint32_t J2 = (CPU.op >> 11) & 0x1;
            uint32_t imm11 = CPU.op & 0x7FF;
            uint32_t offset = (S << 24) | ((~(J1 ^ S)) << 23) | ((~(J2 ^ S)) << 22) | (imm10 << 12) | (imm11 << 1);
            offset = (offset ^ (1 << 25)) - (1 << 25); // Sign-extend

            CPU.REG.PC += offset;
            CPU.REG.PC += 4;
            if (CPU.REG.PC & 1)
            {
                M4_DEBUG("[ERROR] Unaligned PC after B.W: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 4);
                CPU.error = -1;
                return -1;
            }
            return 0;
        }
        else if (op2 == 0b0000010 || op2 == 0b0000011) // TBB, TBH
        {
            uint32_t Rn = (CPU.op >> 16) & 0xF;
            uint32_t Rm = (CPU.op >> 0) & 0xF;
            uint32_t H = (CPU.op >> 4) & 0x1;
            uint32_t address = CPU.REG.r[Rn];
            uint32_t index = CPU.REG.r[Rm];

            if (H && (address & 1)) // TBH requires 2-byte alignment
            {
                M4_DEBUG("[ERROR] Unaligned address for TBH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }

            uint32_t table_address = address + (H ? (index << 1) : index);
            if (table_address >= CPU.RAM_SIZE || (H && table_address + 1 >= CPU.RAM_SIZE))
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", table_address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }

            uint32_t offset;
            if (H) // TBH
            {
                offset = *(uint16_t *)(CPU.RAM + table_address);
                offset <<= 1;
            }
            else // TBB
            {
                offset = *(uint8_t *)(CPU.RAM + table_address);
                offset <<= 1;
            }

            CPU.REG.PC += offset + 4;
            if (CPU.REG.PC & 1)
            {
                M4_DEBUG("[ERROR] Unaligned PC after TBB/TBH: 0x%08X at PC: 0x%08X\n", CPU.REG.PC, CPU.REG.PC - 4);
                CPU.error = -1;
                return -1;
            }
            return 0;
        }
        M4_DEBUG("[ERROR] Unknown branch op: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 4;
        return -1;
    }

    case 0b10000:
    case 0b10001:
    case 0b10010:
    case 0b10011: // Load/Store Single Data
    {
        uint32_t op2 = (CPU.op >> 23) & 0xF;
        uint32_t Rn = (CPU.op >> 16) & 0xF;
        uint32_t Rt = (CPU.op >> 12) & 0xF;
        uint32_t imm12 = CPU.op & 0xFFF;
        uint32_t address;

        if (op2 == 0b1100 || op2 == 0b1110) // LDR, LDRB
        {
            address = CPU.REG.r[Rn] + imm12;
            if (op2 == 0b1100 && (address & 3)) // LDR requires 4-byte alignment
            {
                M4_DEBUG("[ERROR] Unaligned address for LDR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (address >= CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (op2 == 0b1100) // LDR
                CPU.REG.r[Rt] = *(uint32_t *)(CPU.RAM + address);
            else // LDRB
                CPU.REG.r[Rt] = *(uint8_t *)(CPU.RAM + address);
        }
        else if (op2 == 0b1101 || op2 == 0b1111) // STR, STRB
        {
            address = CPU.REG.r[Rn] + imm12;
            if (op2 == 0b1101 && (address & 3)) // STR requires 4-byte alignment
            {
                M4_DEBUG("[ERROR] Unaligned address for STR: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (address >= CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (op2 == 0b1101) // STR
                *(uint32_t *)(CPU.RAM + address) = CPU.REG.r[Rt];
            else // STRB
                *(uint8_t *)(CPU.RAM + address) = CPU.REG.r[Rt] & 0xFF;
        }
        else if (op2 == 0b1000 || op2 == 0b1010) // LDRH, LDRSB
        {
            address = CPU.REG.r[Rn] + imm12;
            if (op2 == 0b1000 && (address & 1)) // LDRH requires 2-byte alignment
            {
                M4_DEBUG("[ERROR] Unaligned address for LDRH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (address >= CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (op2 == 0b1000) // LDRH
                CPU.REG.r[Rt] = *(uint16_t *)(CPU.RAM + address);
            else // LDRSB
                CPU.REG.r[Rt] = (int8_t)*(uint8_t *)(CPU.RAM + address);
        }
        else if (op2 == 0b1001 || op2 == 0b1011) // STRH, LDRSH
        {
            address = CPU.REG.r[Rn] + imm12;
            if (op2 == 0b1001 && (address & 1)) // STRH requires 2-byte alignment
            {
                M4_DEBUG("[ERROR] Unaligned address for STRH: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (address >= CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            if (op2 == 0b1001) // STRH
                *(uint16_t *)(CPU.RAM + address) = CPU.REG.r[Rt] & 0xFFFF;
            else // LDRSH
                CPU.REG.r[Rt] = (int16_t)*(uint16_t *)(CPU.RAM + address);
        }
        else
        {
            M4_DEBUG("[ERROR] Unknown load/store op: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 4;
            return -1;
        }
        CPU.REG.PC += 4;
        return 0;
    }

    case 0b10100:
    case 0b10101: // Load Multiple, Store Multiple, Load/Store Dual
    {
        uint32_t op2 = (CPU.op >> 23) & 0xF;
        uint32_t Rn = (CPU.op >> 16) & 0xF;
        uint32_t reg_list = CPU.op & 0xFFFF;
        uint32_t address = CPU.REG.r[Rn];
        int i, count = 0;

        if (op2 == 0b0001 || op2 == 0b0011) // LDM, STM
        {
            for (i = 0; i < 16; i++)
                if (reg_list & (1 << i))
                    count++;

            if (count == 0)
            {
                M4_DEBUG("[ERROR] Empty register list for LDM/STM: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }

            if (address & 3) // 4-byte alignment
            {
                M4_DEBUG("[ERROR] Unaligned address for LDM/STM: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }

            if (address + count * 4 > CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }

            if (op2 == 0b0001) // STM
            {
                for (i = 0; i < 16; i++)
                {
                    if (reg_list & (1 << i))
                    {
                        *(uint32_t *)(CPU.RAM + address) = CPU.REG.r[i];
                        address += 4;
                    }
                }
                if ((CPU.op >> 20) & 1) // Write-back
                    CPU.REG.r[Rn] = address;
            }
            else // LDM
            {
                for (i = 0; i < 16; i++)
                {
                    if (reg_list & (1 << i))
                    {
                        CPU.REG.r[i] = *(uint32_t *)(CPU.RAM + address);
                        address += 4;
                    }
                }
                if ((CPU.op >> 20) & 1) // Write-back
                    CPU.REG.r[Rn] = address;
            }
        }
        else if (op2 == 0b0100 || op2 == 0b0101) // LDRD, STRD
        {
            uint32_t Rt = (CPU.op >> 12) & 0xF;
            uint32_t Rt2 = (CPU.op >> 8) & 0xF;
            uint32_t imm8 = CPU.op & 0xFF;
            address = CPU.REG.r[Rn] + (imm8 << 2);

            if (address & 3) // 4-byte alignment
            {
                M4_DEBUG("[ERROR] Unaligned address for LDRD/STRD: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }

            if (address + 8 > CPU.RAM_SIZE)
            {
                M4_DEBUG("[ERROR] Invalid RAM access: 0x%08X at PC: 0x%08X\n", address, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }

            if (op2 == 0b0100) // STRD
            {
                *(uint32_t *)(CPU.RAM + address) = CPU.REG.r[Rt];
                *(uint32_t *)(CPU.RAM + address + 4) = CPU.REG.r[Rt2];
            }
            else // LDRD
            {
                CPU.REG.r[Rt] = *(uint32_t *)(CPU.RAM + address);
                CPU.REG.r[Rt2] = *(uint32_t *)(CPU.RAM + address + 4);
            }
        }
        else
        {
            M4_DEBUG("[ERROR] Unknown load/store multiple op: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 4;
            return -1;
        }
        CPU.REG.PC += 4;
        return 0;
    }

    case 0b11000:
    case 0b11001:
    case 0b11010:
    case 0b11011: // Data Processing (Plain Binary Immediate)
    {
        uint32_t op2 = (CPU.op >> 20) & 0x1F;
        uint32_t Rn = (CPU.op >> 16) & 0xF;
        uint32_t Rd = (CPU.op >> 8) & 0xF;
        uint32_t i = (CPU.op >> 26) & 0x1;
        uint32_t imm3 = (CPU.op >> 12) & 0x7;
        uint32_t imm8 = CPU.op & 0xFF;
        uint32_t imm12;
        if (i == 0)
            imm12 = (imm3 << 8) | imm8;
        else
        {
            uint32_t unrotated = imm8;
            if (imm3 == 0b000)
                imm12 = unrotated;
            else if (imm3 == 0b001)
                imm12 = (unrotated << 16) | unrotated;
            else if (imm3 == 0b010)
                imm12 = (unrotated << 8) | (unrotated << 24);
            else if (imm3 == 0b011)
                imm12 = (unrotated << 24) | (unrotated << 16) | (unrotated << 8) | unrotated;
            else
                imm12 = (unrotated << (32 - imm3)) | (unrotated >> imm3);
        }
        uint32_t S = (CPU.op >> 20) & 0x1;

        switch (op2)
        {
        case 0b00000: // ADD immediate
            result = CPU.REG.r[Rn] + imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_ADD, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b00100: // CMP immediate
            result = CPU.REG.r[Rn] - imm12;
            m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_CMP, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b00110: // CMN immediate
            result = CPU.REG.r[Rn] + imm12;
            m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_CMN, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b01010: // SUB immediate
            result = CPU.REG.r[Rn] - imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_SUB, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b01100: // MOV immediate
            result = imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, result, 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10000: // AND immediate
            result = CPU.REG.r[Rn] & imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_AND, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10010: // TST immediate
            result = CPU.REG.r[Rn] & imm12;
            m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_TST, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10100: // ORR immediate
            result = CPU.REG.r[Rn] | imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_ORR, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10101: // TEQ immediate
            result = CPU.REG.r[Rn] ^ imm12;
            m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_TEQ, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10110: // EOR immediate
            result = CPU.REG.r[Rn] ^ imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_EOR, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b11010: // BIC immediate
            result = CPU.REG.r[Rn] & ~imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], imm12, OP_BIC, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b11100: // MVN immediate
            result = ~imm12;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, 0, imm12, OP_MVN, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b11110: // BFC
        {
            uint32_t lsb = (CPU.op >> 16) & 0x1F;
            uint32_t width = ((CPU.op >> 21) & 0x1F) + 1;
            uint32_t mask = (1U << width) - 1;
            CPU.REG.r[Rd] = CPU.REG.r[Rd] & ~(mask << lsb);
            break;
        }
        case 0b11111: // BFI
        {
            uint32_t lsb = (CPU.op >> 16) & 0x1F;
            uint32_t width = ((CPU.op >> 21) & 0x1F) + 1;
            uint32_t mask = (1U << width) - 1;
            CPU.REG.r[Rd] = (CPU.REG.r[Rd] & ~(mask << lsb)) | ((CPU.REG.r[Rn] & mask) << lsb);
            break;
        }
        default:
            M4_DEBUG("[ERROR] Unknown data processing immediate op: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 4;
            return -1;
        }
        CPU.REG.PC += 4;
        return 0;
    }

    case 0b11110:
    case 0b11111: // Data Processing (Modified Immediate, Register)
    {
        uint32_t op2 = (CPU.op >> 20) & 0x1F;
        uint32_t Rn = (CPU.op >> 16) & 0xF;
        uint32_t Rd = (CPU.op >> 8) & 0xF;
        uint32_t Rm = CPU.op & 0xF;
        uint32_t imm5 = (CPU.op >> 6) & 0x1F;
        uint32_t shift_type = (CPU.op >> 4) & 0x3;
        uint32_t S = (CPU.op >> 20) & 0x1;
        uint32_t shifted_value = CPU.REG.r[Rm];

        if (imm5 != 0)
        {
            switch (shift_type)
            {
            case 0b00: // LSL
                shifted_value = CPU.REG.r[Rm] << imm5;
                break;
            case 0b01: // LSR
                shifted_value = CPU.REG.r[Rm] >> imm5;
                break;
            case 0b10: // ASR
                shifted_value = ((int32_t)CPU.REG.r[Rm]) >> imm5;
                break;
            case 0b11: // ROR
                shifted_value = (CPU.REG.r[Rm] >> imm5) | (CPU.REG.r[Rm] << (32 - imm5));
                break;
            }
        }

        switch (op2)
        {
        case 0b00000: // ADD register
            result = CPU.REG.r[Rn] + shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_ADD, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b00010: // ADC register
            result = CPU.REG.r[Rn] + shifted_value + CPU.psr.apsr.C;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_ADC, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b00100: // CMP register
            result = CPU.REG.r[Rn] - shifted_value;
            m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_CMP, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b00110: // CMN register
            result = CPU.REG.r[Rn] + shifted_value;
            m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_CMN, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b01000:                               // MUL
            result = CPU.REG.r[Rn] * CPU.REG.r[Rm]; // MUL does not use shift
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], CPU.REG.r[Rm], OP_MUL, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b01001: // RSB register
            result = shifted_value - CPU.REG.r[Rn];
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, shifted_value, CPU.REG.r[Rn], OP_RSB, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b01010: // SUB register
            result = CPU.REG.r[Rn] - shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_SUB, 0, UPDATE_N | UPDATE_Z | UPDATE_C | UPDATE_V);
            break;
        case 0b01100: // MOV register
            result = shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, result, 0, OP_MOV, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b01110: // LSL register
            result = CPU.REG.r[Rn] << (CPU.REG.r[Rm] & 0xFF);
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_LSL, CPU.REG.r[Rm], UPDATE_N | UPDATE_Z | UPDATE_C);
            break;
        case 0b10000: // AND register
            result = CPU.REG.r[Rn] & shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_AND, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10010: // TST register
            result = CPU.REG.r[Rn] & shifted_value;
            m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_TST, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10100: // ORR register
            result = CPU.REG.r[Rn] | shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_ORR, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10101: // TEQ register
            result = CPU.REG.r[Rn] ^ shifted_value;
            m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_TEQ, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b10110: // EOR register
            result = CPU.REG.r[Rn] ^ shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_EOR, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b11000: // ROR register
            if (CPU.REG.r[Rm] & 0xFF)
            {
                result = (CPU.REG.r[Rn] >> (CPU.REG.r[Rm] & 0xFF)) | (CPU.REG.r[Rn] << (32 - (CPU.REG.r[Rm] & 0xFF)));
                CPU.REG.r[Rd] = result;
                if (S)
                    m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_ROR, CPU.REG.r[Rm], UPDATE_N | UPDATE_Z | UPDATE_C);
            }
            else
            {
                result = CPU.REG.r[Rn];
                CPU.REG.r[Rd] = result;
                if (S)
                    m4_update_apsr(result, CPU.REG.r[Rn], 0, OP_ROR, 0, UPDATE_N | UPDATE_Z);
            }
            break;
        case 0b11010: // BIC register
            result = CPU.REG.r[Rn] & ~shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, CPU.REG.r[Rn], shifted_value, OP_BIC, 0, UPDATE_N | UPDATE_Z);
            break;
        case 0b11100: // MVN register
            result = ~shifted_value;
            CPU.REG.r[Rd] = result;
            if (S)
                m4_update_apsr(result, 0, shifted_value, OP_MVN, 0, UPDATE_N | UPDATE_Z);
            break;
        default:
            M4_DEBUG("[ERROR] Unknown data processing register op: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 4;
            return -1;
        }
        CPU.REG.PC += 4;
        return 0;
    }

    case 0b10110:
    case 0b10111: // Miscellaneous (CLZ, RBIT, etc.)
    {
        uint32_t op2 = (CPU.op >> 20) & 0x7F;
        uint32_t Rd = (CPU.op >> 8) & 0xF;
        uint32_t Rm = CPU.op & 0xF;
        uint32_t Rn = (CPU.op >> 16) & 0xF;

        switch (op2)
        {
        case 0b0000000: // NOP
            break;
        case 0b0001000: // REV
            CPU.REG.r[Rd] = ((CPU.REG.r[Rm] & 0xFF) << 24) |
                            ((CPU.REG.r[Rm] & 0xFF00) << 8) |
                            ((CPU.REG.r[Rm] & 0xFF0000) >> 8) |
                            ((CPU.REG.r[Rm] & 0xFF000000) >> 24);
            break;
        case 0b0001001: // REV16
            CPU.REG.r[Rd] = ((CPU.REG.r[Rm] & 0xFF) << 8) |
                            ((CPU.REG.r[Rm] & 0xFF00) >> 8) |
                            ((CPU.REG.r[Rm] & 0xFF0000) << 8) |
                            ((CPU.REG.r[Rm] & 0xFF000000) >> 8);
            break;
        case 0b0001011: // REVSH
            CPU.REG.r[Rd] = (int16_t)(((CPU.REG.r[Rm] & 0xFF) << 8) |
                                      ((CPU.REG.r[Rm] & 0xFF00) >> 8));
            break;
        case 0b0001010: // CLZ
        {
            uint32_t value = CPU.REG.r[Rm];
            int count = 0;
            while (value != 0 && count < 32)
            {
                if (value & 0x80000000)
                    break;
                value <<= 1;
                count++;
            }
            CPU.REG.r[Rd] = count;
            break;
        }
        case 0b0001100: // RBIT
        {
            uint32_t value = CPU.REG.r[Rm];
            uint32_t reversed = 0;
            for (int i = 0; i < 32; i++)
            {
                reversed |= ((value >> i) & 1) << (31 - i);
            }
            CPU.REG.r[Rd] = reversed;
            break;
        }
        case 0b0010000: // SDIV
        {
            int32_t dividend = (int32_t)CPU.REG.r[Rn];
            int32_t divisor = (int32_t)CPU.REG.r[Rm];
            if (divisor == 0)
            {
                M4_DEBUG("[ERROR] Division by zero in SDIV: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            CPU.REG.r[Rd] = dividend / divisor;
            break;
        }
        case 0b0010001: // UDIV
        {
            if (CPU.REG.r[Rm] == 0)
            {
                M4_DEBUG("[ERROR] Division by zero in UDIV: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
                CPU.error = -1;
                CPU.REG.PC += 4;
                return -1;
            }
            CPU.REG.r[Rd] = CPU.REG.r[Rn] / CPU.REG.r[Rm];
            break;
        }
        case 0b0010010: // SBFX
        {
            uint32_t lsb = (CPU.op >> 6) & 0x1F;
            uint32_t width = ((CPU.op >> 16) & 0x1F) + 1;
            int32_t value = (int32_t)CPU.REG.r[Rm] >> lsb;
            if (width < 32)
                value = (value << (32 - width)) >> (32 - width); // Sign-extend
            CPU.REG.r[Rd] = value;
            break;
        }
        case 0b0010011: // UBFX
        {
            uint32_t lsb = (CPU.op >> 6) & 0x1F;
            uint32_t width = ((CPU.op >> 16) & 0x1F) + 1;
            uint32_t value = CPU.REG.r[Rm] >> lsb;
            if (width < 32)
                value &= (1U << width) - 1; // Zero-extend
            CPU.REG.r[Rd] = value;
            break;
        }
        default:
            M4_DEBUG("[ERROR] Unknown miscellaneous op: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
            CPU.error = -1;
            CPU.REG.PC += 4;
            return -1;
        }
        CPU.REG.PC += 4;
        return 0;
    }

    default:
        M4_DEBUG("[ERROR] Unknown instruction: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 4;
        return -1;
    }
}
