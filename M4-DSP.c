#include "M4.h"
#include "common.h"

#if USE_DSP

// Обработва DSP инструкции за умножение: SMULL, SMLAL, UMAAL.
// Връща 0 при успех, -1 при грешка (невалидни регистри, неподдържана инструкция).
static int handle_dsp_multiply(uint32_t op, uint32_t *pc)
{
    uint8_t RdLo = (op >> 12) & 0xF;
    uint8_t RdHi = (op >> 8) & 0xF;
    uint8_t Rn = (op >> 16) & 0xF;
    uint8_t Rm = op & 0xF;
    uint8_t op2 = (op >> 4) & 0xF;

    // Проверка за валидност на регистри
    if (RdLo == 15 || RdHi == 15 || Rn == 15 || Rm == 15 || RdLo == RdHi)
    {
        M4_DEBUG("[ERROR] Invalid registers: RdLo=%u, RdHi=%u, Rn=%u, Rm=%u, op=0x%08X at PC: 0x%08X\n",
                 RdLo, RdHi, Rn, Rm, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    switch (op2)
    {
    case 0b0000: // SMULL (Signed Multiply Long)
    {
        // Забележка: SMULL не обновява APSR флагове
        int64_t result = (int64_t)(int32_t)CPU.REG.r[Rn] * (int64_t)(int32_t)CPU.REG.r[Rm];
        CPU.REG.r[RdLo] = (uint32_t)(result & 0xFFFFFFFF);
        CPU.REG.r[RdHi] = (uint32_t)(result >> 32);
        break;
    }
    case 0b0100: // SMLAL (Signed Multiply-Accumulate Long)
    {
        // Забележка: SMLAL не обновява APSR флагове
        int64_t product = (int64_t)(int32_t)CPU.REG.r[Rn] * (int64_t)(int32_t)CPU.REG.r[Rm];
        uint64_t accum = ((uint64_t)CPU.REG.r[RdHi] << 32) | CPU.REG.r[RdLo];
        uint64_t result = accum + product;
        CPU.REG.r[RdLo] = (uint32_t)(result & 0xFFFFFFFF);
        CPU.REG.r[RdHi] = (uint32_t)(result >> 32);
        break;
    }
    case 0b0110: // UMAAL (Unsigned Multiply-Accumulate-Accumulate Long)
    {
        // Забележка: UMAAL не обновява APSR флагове
        uint64_t product = (uint64_t)CPU.REG.r[Rn] * (uint64_t)CPU.REG.r[Rm];
        uint64_t accum = ((uint64_t)CPU.REG.r[RdHi] << 32) | CPU.REG.r[RdLo];
        uint64_t result = product + accum + CPU.REG.r[RdLo];
        CPU.REG.r[RdLo] = (uint32_t)(result & 0xFFFFFFFF);
        CPU.REG.r[RdHi] = (uint32_t)(result >> 32);
        break;
    }
    default:
        M4_DEBUG("[ERROR] Unknown DSP multiply op: op=0x%08X at PC: 0x%08X\n", op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    *pc += 4;
    return 0;
}

// Обработва DSP инструкции за насищаща аритметика: QADD, QSUB, QDADD, QDSUB.
// Връща 0 при успех, -1 при грешка (невалидни регистри, неподдържана инструкция).
static int handle_dsp_saturating(uint32_t op, uint32_t *pc)
{
    uint8_t Rd = (op >> 12) & 0xF;
    uint8_t Rn = (op >> 16) & 0xF;
    uint8_t Rm = op & 0xF;
    uint8_t op2 = (op >> 4) & 0xF;

    // Проверка за валидност на регистри
    if (Rd == 15 || Rn == 15 || Rm == 15)
    {
        M4_DEBUG("[ERROR] Invalid registers: Rd=%u, Rn=%u, Rm=%u, op=0x%08X at PC: 0x%08X\n",
                 Rd, Rn, Rm, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    switch (op2)
    {
    case 0b0000: // QADD
    {
        int64_t result = (int64_t)(int32_t)CPU.REG.r[Rn] + (int64_t)(int32_t)CPU.REG.r[Rm];
        if (result > INT32_MAX)
        {
            CPU.REG.r[Rd] = INT32_MAX;
            CPU.psr.apsr.Q = 1; // Насищане
        }
        else if (result < INT32_MIN)
        {
            CPU.REG.r[Rd] = INT32_MIN;
            CPU.psr.apsr.Q = 1; // Насищане
        }
        else
        {
            CPU.REG.r[Rd] = (int32_t)result;
        }
        break;
    }
    case 0b0001: // QSUB
    {
        int64_t result = (int64_t)(int32_t)CPU.REG.r[Rn] - (int64_t)(int32_t)CPU.REG.r[Rm];
        if (result > INT32_MAX)
        {
            CPU.REG.r[Rd] = INT32_MAX;
            CPU.psr.apsr.Q = 1;
        }
        else if (result < INT32_MIN)
        {
            CPU.REG.r[Rd] = INT32_MIN;
            CPU.psr.apsr.Q = 1;
        }
        else
        {
            CPU.REG.r[Rd] = (int32_t)result;
        }
        break;
    }
    case 0b0010: // QDADD
    {
        int64_t doubled = (int64_t)(int32_t)CPU.REG.r[Rm] * 2;
        if (doubled > INT32_MAX)
        {
            doubled = INT32_MAX;
            CPU.psr.apsr.Q = 1;
        }
        else if (doubled < INT32_MIN)
        {
            doubled = INT32_MIN;
            CPU.psr.apsr.Q = 1;
        }
        int64_t result = (int64_t)(int32_t)CPU.REG.r[Rn] + doubled;
        if (result > INT32_MAX)
        {
            CPU.REG.r[Rd] = INT32_MAX;
            CPU.psr.apsr.Q = 1;
        }
        else if (result < INT32_MIN)
        {
            CPU.REG.r[Rd] = INT32_MIN;
            CPU.psr.apsr.Q = 1;
        }
        else
        {
            CPU.REG.r[Rd] = (int32_t)result;
        }
        break;
    }
    case 0b0011: // QDSUB
    {
        int64_t doubled = (int64_t)(int32_t)CPU.REG.r[Rm] * 2;
        if (doubled > INT32_MAX)
        {
            doubled = INT32_MAX;
            CPU.psr.apsr.Q = 1;
        }
        else if (doubled < INT32_MIN)
        {
            doubled = INT32_MIN;
            CPU.psr.apsr.Q = 1;
        }
        int64_t result = (int64_t)(int32_t)CPU.REG.r[Rn] - doubled;
        if (result > INT32_MAX)
        {
            CPU.REG.r[Rd] = INT32_MAX;
            CPU.psr.apsr.Q = 1;
        }
        else if (result < INT32_MIN)
        {
            CPU.REG.r[Rd] = INT32_MIN;
            CPU.psr.apsr.Q = 1;
        }
        else
        {
            CPU.REG.r[Rd] = (int32_t)result;
        }
        break;
    }
    default:
        M4_DEBUG("[ERROR] Unknown DSP saturating op: op=0x%08X at PC: 0x%08X\n", op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    *pc += 4;
    return 0;
}

// Обработва DSP инструкции за пакетиране и разширяване: PKHBT, PKHTB, SXTB, UXTB, SXTH, UXTH.
// Връща 0 при успех, -1 при грешка (невалидни регистри, неподдържана инструкция).
static int handle_dsp_pack_extend(uint32_t op, uint32_t *pc)
{
    uint8_t Rd = (op >> 12) & 0xF;
    uint8_t Rn = (op >> 16) & 0xF;
    uint8_t Rm = op & 0xF;
    uint8_t op2 = (op >> 4) & 0xF;
    uint8_t rotation = (op >> 6) & 0x3; // За SXT/UXT
    uint8_t tb = (op >> 5) & 0x1;       // За PKHBT/PKHTB

    // Проверка за валидност на регистри
    if (Rd == 15 || Rn == 15 || Rm == 15)
    {
        M4_DEBUG("[ERROR] Invalid registers: Rd=%u, Rn=%u, Rm=%u, op=0x%08X at PC: 0x%08X\n",
                 Rd, Rn, Rm, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    // Проверка за валидност на rotation
    if (rotation > 3)
    {
        M4_DEBUG("[ERROR] Invalid rotation: rotation=%u, op=0x%08X at PC: 0x%08X\n", rotation, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    switch (op2)
    {
    case 0b0000: // PKHBT (Pack Halfword Bottom-Top)
    {
        uint32_t shifted_Rm = (rotation == 0) ? CPU.REG.r[Rm] : (CPU.REG.r[Rm] << (rotation * 8));
        CPU.REG.r[Rd] = (CPU.REG.r[Rn] & 0xFFFF) | (shifted_Rm & 0xFFFF0000);
        break;
    }
    case 0b0001: // PKHTB (Pack Halfword Top-Bottom)
    {
        uint32_t shifted_Rm = (rotation == 0) ? CPU.REG.r[Rm] : (CPU.REG.r[Rm] >> (rotation * 8));
        CPU.REG.r[Rd] = (shifted_Rm & 0xFFFF) | (CPU.REG.r[Rn] & 0xFFFF0000);
        break;
    }
    case 0b0100: // SXTB (Sign-Extend Byte)
    {
        uint32_t value = rotation ? ((CPU.REG.r[Rm] >> (rotation * 8)) & 0xFF) : (CPU.REG.r[Rm] & 0xFF);
        CPU.REG.r[Rd] = (int32_t)(int8_t)value;
        break;
    }
    case 0b0101: // UXTB (Zero-Extend Byte)
    {
        uint32_t value = rotation ? ((CPU.REG.r[Rm] >> (rotation * 8)) & 0xFF) : (CPU.REG.r[Rm] & 0xFF);
        CPU.REG.r[Rd] = value;
        break;
    }
    case 0b0110: // SXTH (Sign-Extend Halfword)
    {
        uint32_t value = rotation ? ((CPU.REG.r[Rm] >> (rotation * 8)) & 0xFFFF) : (CPU.REG.r[Rm] & 0xFFFF);
        CPU.REG.r[Rd] = (int32_t)(int16_t)value;
        break;
    }
    case 0b0111: // UXTH (Zero-Extend Halfword)
    {
        uint32_t value = rotation ? ((CPU.REG.r[Rm] >> (rotation * 8)) & 0xFFFF) : (CPU.REG.r[Rm] & 0xFFFF);
        CPU.REG.r[Rd] = value;
        break;
    }
    default:
        M4_DEBUG("[ERROR] Unknown DSP pack/extend op: op=0x%08X at PC: 0x%08X\n", op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    *pc += 4;
    return 0;
}

// Изпълнява 32-битова DSP инструкция за Cortex-M4 (SIMD и насищащи операции).
// Връща 0 при успех, -1 при грешка (невалиден PC, неподдържана инструкция, неинициализирана памет).
int m4_execute_DSP(void)
{
    // Проверка за инициализация на паметта
    if (!CPU.RAM || !CPU.ROM || CPU.RAM_SIZE == 0 || CPU.ROM_SIZE == 0)
    {
        M4_DEBUG("[ERROR] CPU memory not initialized: RAM=%p, ROM=%p, RAM_SIZE=%u, ROM_SIZE=%u\n",
                 CPU.RAM, CPU.ROM, CPU.RAM_SIZE, CPU.ROM_SIZE);
        CPU.error = -1;
        return -1;
    }

    // Проверка за граници на ROM
    if (CPU.REG.PC + 3 >= CPU.ROM_SIZE)
    {
        M4_DEBUG("[ERROR] Invalid PC access: PC=0x%08X, op=0x%08X\n", CPU.REG.PC, CPU.op);
        CPU.error = -1;
        return -1;
    }

    // Проверка за невалидна инструкция
    if (CPU.op == 0)
    {
        M4_DEBUG("[ERROR] Invalid instruction: op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 4;
        return -1;
    }

    // Проверка за Thumb режим
    if (!CPU.psr.epsr.T)
    {
        M4_DEBUG("[ERROR] ARM mode not supported (EPSR.T=0): op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    // Проверка за активиран DSP
    if (!CPU.dsp_enabled) // Предполага се, че M4.h дефинира CPU.dsp_enabled
    {
        M4_DEBUG("[ERROR] DSP not enabled: op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    // Проверка за DSP инструкции
    if ((CPU.op & 0xFF000000) != 0xFA000000)
    {
        M4_DEBUG("[ERROR] Not a DSP instruction: op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 4;
        return -1;
    }

    uint8_t op1 = (CPU.op >> 20) & 0x7;
    uint8_t op2 = (op >> 4) & 0xF;

    // Декодиране на основните групи DSP инструкции
    if (op1 == 0b000)
    {
        // Multiply and Multiply-Accumulate (SMULL, SMLAL, etc.)
        return handle_dsp_multiply(CPU.op, &CPU.REG.PC);
    }
    else if (op1 == 0b001)
    {
        // Saturating Arithmetic (QADD, QSUB, etc.)
        return handle_dsp_saturating(CPU.op, &CPU.REG.PC);
    }
    else if (op1 == 0b010)
    {
        // Pack and Extend (PKHBT, PKHTB, SXTB, UXTB, etc.)
        return handle_dsp_pack_extend(CPU.op, &CPU.REG.PC);
    }

    M4_DEBUG("[ERROR] Unknown DSP instruction: op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
    CPU.error = -1;
    CPU.REG.PC += 4;
    return -1;
}

#endif // USE_DSP