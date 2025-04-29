#include "M4.h"
#include "common.h"

#if USE_FPU

// Обработва FPU инструкции за обработка на данни: VADD, VSUB, VMUL, VDIV, VCMP, VMOV.
// Връща 0 при успех, -1 при грешка (невалидни регистри, деление на нула, неподдържана инструкция).
static int handle_vfp_data_processing(uint32_t op, uint32_t *pc)
{
    uint8_t op2 = (op >> 4) & 0x7;
    uint8_t Vd = ((op >> 12) & 0xF) | (((op >> 22) & 0x1) << 4); // Sx регистър
    uint8_t Vn = ((op >> 16) & 0xF) | (((op >> 7) & 0x1) << 4);
    uint8_t Vm = (op & 0xF) | (((op >> 5) & 0x1) << 4);
    uint8_t sz = (op >> 8) & 0x1; // 0 за single-precision, 1 за double-precision
    float result;

    // Проверка за единична точност (Cortex-M4 поддържа само single-precision)
    if (sz != 0)
    {
        M4_DEBUG("[ERROR] Double-precision not supported: op=0x%08X at PC: 0x%08X\n", op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    // Проверка за валидност на регистри
    if (Vd >= 32 || Vn >= 32 || Vm >= 32)
    {
        M4_DEBUG("[ERROR] Invalid FPU register: Vd=%u, Vn=%u, Vm=%u, op=0x%08X at PC: 0x%08X\n", Vd, Vn, Vm, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    switch (op2)
    {
    case 0b000: // VADD.F32
        result = CPU.FP_REG.s[Vn] + CPU.FP_REG.s[Vm];
        CPU.FP_REG.s[Vd] = result;
        break;
    case 0b001: // VSUB.F32
        result = CPU.FP_REG.s[Vn] - CPU.FP_REG.s[Vm];
        CPU.FP_REG.s[Vd] = result;
        break;
    case 0b010: // VMUL.F32
        result = CPU.FP_REG.s[Vn] * CPU.FP_REG.s[Vm];
        CPU.FP_REG.s[Vd] = result;
        break;
    case 0b011: // VDIV.F32
        if (CPU.FP_REG.s[Vm] == 0.0f)
        {
            M4_DEBUG("[ERROR] Division by zero in VDIV: Vn=%f, Vm=%f, op=0x%08X at PC: 0x%08X\n",
                     CPU.FP_REG.s[Vn], CPU.FP_REG.s[Vm], op, *pc);
            CPU.error = -1;
            *pc += 4;
            return -1;
        }
        result = CPU.FP_REG.s[Vn] / CPU.FP_REG.s[Vm];
        CPU.FP_REG.s[Vd] = result;
        break;
    case 0b100: // VCMP.F32
        if (CPU.FP_REG.s[Vn] == CPU.FP_REG.s[Vm])
        {
            CPU.psr.fpscr.Z = 1;
            CPU.psr.fpscr.C = 1;
            CPU.psr.fpscr.N = 0;
            CPU.psr.fpscr.V = 0;
        }
        else if (CPU.FP_REG.s[Vn] < CPU.FP_REG.s[Vm])
        {
            CPU.psr.fpscr.Z = 0;
            CPU.psr.fpscr.C = 0;
            CPU.psr.fpscr.N = 1;
            CPU.psr.fpscr.V = 0;
        }
        else
        {
            CPU.psr.fpscr.Z = 0;
            CPU.psr.fpscr.C = 1;
            CPU.psr.fpscr.N = 0;
            CPU.psr.fpscr.V = 0;
        }
        break;
    case 0b101: // VMOV.F32 (регистър към регистър)
        CPU.FP_REG.s[Vd] = CPU.FP_REG.s[Vm];
        break;
    default:
        M4_DEBUG("[ERROR] Unknown VFP data processing op: op=0x%08X at PC: 0x%08X\n", op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    // Проверка за NaN, безкрайност, underflow и inexact
    if (op2 <= 0b011)
    {
        if (isnan(result))
        {
            CPU.psr.fpscr.IOC = 1; // Invalid Operation
            CPU.FP_REG.s[Vd] = 0.0f;
        }
        else if (isinf(result))
        {
            CPU.psr.fpscr.OFC = 1; // Overflow
            CPU.FP_REG.s[Vd] = 0.0f;
        }
        else if (result != 0.0f && fabsf(result) < FLT_MIN)
        {
            CPU.psr.fpscr.UFC = 1; // Underflow
            CPU.FP_REG.s[Vd] = 0.0f;
        }
        if (result != (float)((int32_t)result))
        {
            CPU.psr.fpscr.IXC = 1; // Inexact
        }
        // Ако е активиран NVIC, предизвиква изключение за IOC
        if (CPU.psr.fpscr.IOC && USE_NVIC)
        {
            M4_DEBUG("[ERROR] FPU exception triggered (IOC): op=0x%08X at PC: 0x%08X\n", op, *pc);
            CPU.error = -1;
            return m4_trigger_exception(FPU_EXCEPTION); // Предполага се, че M4.h дефинира функция
        }
    }

    *pc += 4;
    return 0;
}

// Обработва FPU инструкции за зареждане/запис: VLDR, VSTR.
// Връща 0 при успех, -1 при грешка (невалиден адрес, неподравняване, невалидни регистри).
static int handle_vfp_load_store(uint32_t op, uint32_t *pc)
{
    uint8_t Vd = ((op >> 12) & 0xF) | (((op >> 22) & 0x1) << 4); // Sx регистър
    uint8_t Rn = (op >> 16) & 0xF;
    uint8_t U = (op >> 23) & 0x1; // 1 за добавяне, 0 за изваждане
    uint8_t sz = (op >> 8) & 0x1; // 0 за single-precision
    uint32_t imm8 = op & 0xFF;
    uint32_t imm = imm8 << 2; // Умножение по 4 за байтово отместване

    // Проверка за единична точност
    if (sz != 0)
    {
        M4_DEBUG("[ERROR] Double-precision not supported: op=0x%08X at PC: 0x%08X\n", op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    // Проверка за валидност на регистри
    if (Vd >= 32 || Rn == 15)
    {
        M4_DEBUG("[ERROR] Invalid register: Vd=%u, Rn=%u, op=0x%08X at PC: 0x%08X\n", Vd, Rn, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    // Изчисляване на адрес
    uint64_t temp_address = (uint64_t)CPU.REG.r[Rn] + (U ? imm : -imm);
    if (temp_address > UINT32_MAX || (int64_t)temp_address < 0)
    {
        M4_DEBUG("[ERROR] Address out of range: address=0x%016llX, op=0x%08X at PC: 0x%08X\n", temp_address, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }
    uint32_t address = (uint32_t)temp_address;

    // Проверка за подравняване (4 байта за single-precision)
    if (address & 3 || (uintptr_t)(CPU.RAM + address) & 3)
    {
        M4_DEBUG("[ERROR] Unaligned address for VLDR/VSTR: address=0x%08X, op=0x%08X at PC: 0x%08X\n", address, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    // Проверка за валиден RAM достъп
    if (address + 3 >= CPU.RAM_SIZE)
    {
        M4_DEBUG("[ERROR] Invalid RAM access: address=0x%08X, op=0x%08X at PC: 0x%08X\n", address, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    if ((op >> 20) & 0x1)
    { // VLDR
        memcpy(&CPU.FP_REG.s[Vd], CPU.RAM + address, sizeof(float));
    }
    else
    { // VSTR
        memcpy(CPU.RAM + address, &CPU.FP_REG.s[Vd], sizeof(float));
    }

    *pc += 4;
    return 0;
}

// Обработва FPU инструкции за прехвърляне: VMOV, VMRS, VMSR.
// Връща 0 при успех, -1 при грешка (невалидни регистри, неподдържана инструкция).
static int handle_vfp_move(uint32_t op, uint32_t *pc)
{
    uint8_t Rt = (op >> 12) & 0xF;
    uint8_t Vn = ((op >> 16) & 0xF) | (((op >> 7) & 0x1) << 4);
    uint8_t op2 = (op >> 4) & 0x7;
    uint8_t to_arm = (op >> 20) & 0x1;

    // Проверка за валидност на регистри
    if (Vn >= 32 || (Rt == 15 && op2 != 0b001))
    {
        M4_DEBUG("[ERROR] Invalid register: Vn=%u, Rt=%u, op=0x%08X at PC: 0x%08X\n", Vn, Rt, op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    switch (op2)
    {
    case 0b000: // VMOV (ARM регистър <-> FPU регистър)
    {
        union
        {
            float f;
            uint32_t u;
        } conv;
        if (to_arm)
        {
            conv.f = CPU.FP_REG.s[Vn];
            CPU.REG.r[Rt] = conv.u;
        }
        else
        {
            conv.u = CPU.REG.r[Rt];
            CPU.FP_REG.s[Vn] = conv.f;
        }
        break;
    }
    case 0b001: // VMRS, VMSR
        if (to_arm)
        { // VMRS
            if (Rt == 15)
            { // Прехвърля FPSCR към APSR
                CPU.psr.apsr.N = CPU.psr.fpscr.N;
                CPU.psr.apsr.Z = CPU.psr.fpscr.Z;
                CPU.psr.apsr.C = CPU.psr.fpscr.C;
                CPU.psr.apsr.V = CPU.psr.fpscr.V;
            }
            else
            {
                union
                {
                    float f;
                    uint32_t u;
                } conv;
                conv.f = *(float *)&CPU.psr.fpscr;
                CPU.REG.r[Rt] = conv.u;
            }
        }
        else
        { // VMSR
            union
            {
                float f;
                uint32_t u;
            } conv;
            conv.u = CPU.REG.r[Rt];
            *(float *)&CPU.psr.fpscr = conv.f;
        }
        break;
    default:
        M4_DEBUG("[ERROR] Unknown VFP move op: op=0x%08X at PC: 0x%08X\n", op, *pc);
        CPU.error = -1;
        *pc += 4;
        return -1;
    }

    *pc += 4;
    return 0;
}

// Изпълнява 32-битова FPU инструкция за Cortex-M4 (FPv4-SP).
// Връща 0 при успех, -1 при грешка (невалиден PC, неподдържана инструкция, неинициализирана памет).
int m4_execute_FPU(void)
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

    // Проверка за активиран FPU
    if (!CPU.fpu_enabled) // Предполага се, че M4.h дефинира CPU.fpu_enabled
    {
        M4_DEBUG("[ERROR] FPU not enabled: op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    // Проверка за FPU инструкции
    if ((CPU.op & 0xFF000000) != 0xEE000000 && (CPU.op & 0xFF000000) != 0xEF000000)
    {
        M4_DEBUG("[ERROR] Not an FPU instruction: op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
        CPU.error = -1;
        CPU.REG.PC += 4;
        return -1;
    }

    uint8_t op1 = (CPU.op >> 20) & 0x7F;

    // Декодиране на основните групи FPU инструкции
    if ((CPU.op & 0xFF000000) == 0xEE000000)
    {
        // VFP Data Processing (VADD, VSUB, VMUL, VDIV, VCMP, VMOV)
        if ((op1 & 0b1111000) == 0b0000000)
        {
            return handle_vfp_data_processing(CPU.op, &CPU.REG.PC);
        }
        // VFP Load/Store (VLDR, VSTR)
        else if ((op1 & 0b1111000) == 0b0010000)
        {
            return handle_vfp_load_store(CPU.op, &CPU.REG.PC);
        }
    }
    else if ((CPU.op & 0xFF000000) == 0xEF000000)
    {
        // VFP Move (VMOV, VMRS, VMSR)
        if ((op1 & 0b1111000) == 0b0111000)
        {
            return handle_vfp_move(CPU.op, &CPU.REG.PC);
        }
    }

    M4_DEBUG("[ERROR] Unknown FPU instruction: op=0x%08X at PC: 0x%08X\n", CPU.op, CPU.REG.PC);
    CPU.error = -1;
    CPU.REG.PC += 4;
    return -1;
}

#endif // USE_FPU