#include "m4.h"
#include "common.h"

int m4_execute_32(void)
{
    FUNC_VM();

    int res = 0;

    // Проверка за DSP инструкции
    if ((CPU.op & 0xFF800000) == 0xF3800000 || (CPU.op & 0xFF800000) == 0xF3C00000)
    {
        DEBUG_M4("[ERROR] DSP instruction not supported yet: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        RETURN_ERROR(-1);
    }
    // Проверка за FPU инструкции
    else if ((CPU.op & 0xEE000000) == 0xEE000000)
    {
        DEBUG_M4("[ERROR] FPU instruction not supported yet: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        RETURN_ERROR(-1);
    }

    uint8_t op = (CPU.op >> 27) & 0x1F;
    DEBUG_M4("[V] INSTRUCTION [32]: 0x%08X, OP: %d\n", CPU.op, op);

    switch (op)
    {
    case 0b11110: // Branch with Link (BL)
    {
        PRINTF("\tBL <link>\n");
        uint32_t S = (CPU.op >> 26) & 0x1;
        uint32_t imm10 = (CPU.op >> 16) & 0x3FF;
        uint32_t J1 = (CPU.op >> 13) & 0x1;
        uint32_t J2 = (CPU.op >> 11) & 0x1;
        uint32_t imm11 = CPU.op & 0x7FF;

        // Изчисляване на I1 и I2
        uint32_t I1 = ~(J1 ^ S);
        uint32_t I2 = ~(J2 ^ S);

        // Формиране на 25-битов офсет
        uint32_t offset = (S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);

        // Sign-extension до 32 бита
        int32_t signed_offset;
        if (S)
            signed_offset = (int32_t)(offset | 0xFE000000); // Запълваме с 1 за отрицателни
        else
            signed_offset = (int32_t)(offset & 0x01FFFFFF); // Запълваме с 0 за положителни

        CPU.REG.LR = CPU.REG.PC + 4;
        uint32_t new_pc = CPU.REG.PC + signed_offset + 4; // някак си е правилно +4 ????

        DEBUG_M4("[BL] Current PC: 0x%08X, Offset: 0x%08X (%d)\n", CPU.REG.PC, signed_offset, signed_offset);
        if (new_pc & 0x1) {
            DEBUG_M4("[ERROR] Unaligned target address for BL: 0x%08X at PC: 0x%08X\n", new_pc, CPU.REG.PC);
            res = -1;
        } else if (new_pc < ROM_BASE || new_pc >= ROM_BASE + CPU.ROM_SIZE) {
            DEBUG_M4("[ERROR] Out-of-bounds PC after BL: 0x%08X at PC: 0x%08X\n", new_pc, CPU.REG.PC);
            res = -1;
        }
        else
        {
            CPU.REG.PC = new_pc;
            DEBUG_M4("[BL] New PC: 0x%08X\n", CPU.REG.PC);
        }
        break;
    }
    default:
        DEBUG_M4("[ERROR] Unsupported instruction: 0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        res = -1;
        break;
    }

    if (res == 0)
    {
        PRINT_REG();
        if (op != 0b11110) // Не увеличаваме PC за BL
            CPU.REG.PC += 4;
    }
    else
    {
        PRINT_REG();
    }

    RETURN_ERROR(res);
}