#include "M4.h"

// Глобална CPU структура
CortexM4 CPU;

// Помощна функция за изчисляване на Overflow (V) за ADD и CMN
static int compute_overflow_add(int32_t s_op1, int32_t s_op2, int32_t s_result)
{
    return ((s_op1 > 0 && s_op2 > 0 && s_result < 0) || (s_op1 < 0 && s_op2 < 0 && s_result > 0)) ? 1 : 0;
}

// Функция за ADD и ADC
static void update_flags_add_adc(uint32_t op1, uint32_t op2, uint32_t result, int operation_type, int update_flags)
{
    if (update_flags & UPDATE_C)
    {
        CPU.psr.apsr.C = ((uint64_t)op1 + op2 + (operation_type == OP_ADC ? CPU.psr.apsr.C : 0) > 0xFFFFFFFF) ? 1 : 0;
    }
    if (update_flags & UPDATE_V)
    {
        int32_t s_op1 = (int32_t)op1, s_op2 = (int32_t)op2, s_result = (int32_t)result;
        CPU.psr.apsr.V = compute_overflow_add(s_op1, s_op2, s_result);
    }
}

// Функция за SUB, SBC и CMP
static void update_flags_sub_sbc_cmp(uint32_t op1, uint32_t op2, uint32_t result, int operation_type, int update_flags)
{
    if (update_flags & UPDATE_C)
    {
        CPU.psr.apsr.C = (op1 >= op2 + (operation_type == OP_SBC ? 1 - CPU.psr.apsr.C : 0)) ? 1 : 0;
    }
    if (update_flags & UPDATE_V)
    {
        int32_t s_op1 = (int32_t)op1, s_op2 = (int32_t)op2, s_result = (int32_t)result;
        CPU.psr.apsr.V = ((s_op1 > 0 && s_op2 < 0 && s_result < 0) || (s_op1 < 0 && s_op2 > 0 && s_result > 0)) ? 1 : 0;
    }
}

// Функция за RSB
static void update_flags_rsb(uint32_t op1, uint32_t op2, uint32_t result, int update_flags)
{
    if (update_flags & UPDATE_C)
    {
        CPU.psr.apsr.C = (op2 >= op1) ? 1 : 0;
    }
    if (update_flags & UPDATE_V)
    {
        int32_t s_op1 = (int32_t)op1, s_op2 = (int32_t)op2, s_result = (int32_t)result;
        CPU.psr.apsr.V = ((s_op2 > 0 && s_op1 < 0 && s_result < 0) || (s_op2 < 0 && s_op1 > 0 && s_result > 0)) ? 1 : 0;
    }
}

// Функция за CMN
static void update_flags_cmn(uint32_t op1, uint32_t op2, uint32_t result, int update_flags)
{
    if (update_flags & UPDATE_C)
    {
        CPU.psr.apsr.C = ((uint64_t)op1 + op2 > 0xFFFFFFFF) ? 1 : 0;
    }
    if (update_flags & UPDATE_V)
    {
        int32_t s_op1 = (int32_t)op1, s_op2 = (int32_t)op2, s_result = (int32_t)result;
        CPU.psr.apsr.V = compute_overflow_add(s_op1, s_op2, s_result);
    }
}

// Функция за LSL
static void update_flags_lsl(uint32_t op1, int shift_amount, int update_flags)
{
    if (update_flags & UPDATE_C)
    {
        if (shift_amount < 0 || shift_amount > 32)
        {
            M4_DEBUG("[ERROR] Invalid shift_amount %d in LSL\n", shift_amount);
            CPU.psr.apsr.C = 0;
            return;
        }
        if (shift_amount > 0)
        {
            CPU.psr.apsr.C = (op1 >> (32 - shift_amount)) & 1;
        }
        else
        {
            CPU.psr.apsr.C = 0; // Няма шифт, C остава непроменен
        }
    }
}

// Функция за LSR, ASR и ROR
static void update_flags_lsr_asr_ror(uint32_t op1, int shift_amount, int update_flags)
{
    if (update_flags & UPDATE_C)
    {
        if (shift_amount < 0 || shift_amount > 32)
        {
            M4_DEBUG("[ERROR] Invalid shift_amount %d in LSR/ASR/ROR\n", shift_amount);
            CPU.psr.apsr.C = 0;
            return;
        }
        if (shift_amount > 0)
        {
            CPU.psr.apsr.C = (op1 >> (shift_amount - 1)) & 1;
        }
        else
        {
            CPU.psr.apsr.C = 0; // Няма шифт, C остава непроменен
        }
    }
}

// Функция за TST и TEQ
static void update_flags_tst_teq(int update_flags)
{
    // C и V не се променят
    (void)update_flags; // Избягва предупреждение за неизползван параметър
}

#if USE_DSP
// Функция за Q флага при SSAT и USAT
static void update_flags_q_ssat_usat(uint32_t result, uint32_t op1, int update_flags)
{
    if (update_flags & UPDATE_Q)
    {
        CPU.psr.apsr.Q = (result != op1) ? 1 : CPU.psr.apsr.Q;
    }
}

// Функция за GE флага при SADD16 и UADD16
static void update_flags_ge_sadd16_uadd16(uint32_t result, uint32_t op1, uint32_t op2, int operation_type, int update_flags)
{
    if (update_flags & UPDATE_GE)
    {
        int16_t op1_high = (op1 >> 16) & 0xFFFF, op1_low = op1 & 0xFFFF;
        int16_t op2_high = (op2 >> 16) & 0xFFFF, op2_low = op2 & 0xFFFF;
        int16_t res_high = (result >> 16) & 0xFFFF, res_low = result & 0xFFFF;
        CPU.psr.apsr.GE = 0;
        if (operation_type == OP_SADD16)
        {
            CPU.psr.apsr.GE |= (res_high >= 0) ? (1 << 3) : 0;
            CPU.psr.apsr.GE |= (res_low >= 0) ? (1 << 2) : 0;
        }
        else
        {
            CPU.psr.apsr.GE |= (res_high >= op1_high + op2_high) ? (1 << 3) : 0;
            CPU.psr.apsr.GE |= (res_low >= op1_low + op2_low) ? (1 << 2) : 0;
        }
    }
}
#endif

// Основна функция за актуализация на APSR
void m4_update_apsr(uint32_t result, uint32_t op1, uint32_t op2, int operation_type, int shift_amount, int update_flags)
{
    // Актуализация на N (Negative)
    if (update_flags & UPDATE_N)
    {
        CPU.psr.apsr.N = (result >> 31) & 1;
    }

    // Актуализация на Z (Zero)
    if (update_flags & UPDATE_Z)
    {
        CPU.psr.apsr.Z = (result == 0) ? 1 : 0;
    }

    // Актуализация на всички флагове (C, V, Q, GE)
    if (update_flags & (UPDATE_C | UPDATE_V | UPDATE_Q | UPDATE_GE))
    {
        switch (operation_type)
        {
        case OP_ADD:
        case OP_ADC:
            update_flags_add_adc(op1, op2, result, operation_type, update_flags);
            break;
        case OP_SUB:
        case OP_SBC:
        case OP_CMP:
            update_flags_sub_sbc_cmp(op1, op2, result, operation_type, update_flags);
            break;
        case OP_RSB:
            update_flags_rsb(op1, op2, result, update_flags);
            break;
        case OP_CMN:
            update_flags_cmn(op1, op2, result, update_flags);
            break;
        case OP_LSL:
            update_flags_lsl(op1, shift_amount, update_flags);
            break;
        case OP_LSR:
        case OP_ASR:
        case OP_ROR:
            update_flags_lsr_asr_ror(op1, shift_amount, update_flags);
            break;
        case OP_TST:
        case OP_TEQ:
            update_flags_tst_teq(update_flags);
            break;
#if USE_DSP
        case OP_SSAT:
        case OP_USAT:
            update_flags_q_ssat_usat(result, op1, update_flags);
            break;
        case OP_SADD16:
        case OP_UADD16:
            update_flags_ge_sadd16_uadd16(result, op1, op2, operation_type, update_flags);
            break;
#endif
        default:
            M4_DEBUG("[WARNING] Unsupported operation_type %d in m4_update_apsr\n", operation_type);
            break;
        }
    }
}

int m4_execute(void)
{
    if (CPU.REG.PC & 0x1)
    {
        M4_DEBUG("[ERROR] Unaligned PC: 0x%08X\n", CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    if (!CPU.psr.epsr.T)
    {
        M4_DEBUG("[ERROR] Invalid Thumb state at PC: 0x%08X\n", CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    if (CPU.REG.PC >= CPU.ROM_SIZE + ROM_BASE)
    {
        M4_DEBUG("[ERROR] Invalid PC access: 0x%08X\n", CPU.REG.PC);
        CPU.error = -1;
        return -1;
    }

    CPU.op = READ_ROM_16(CPU.REG.PC);

    if ((CPU.op & 0xF800) >= 0xE800)
    {
        if (CPU.REG.PC & 0x3)
        {
            M4_DEBUG("[ERROR] Unaligned PC for 32-bit instruction: 0x%08X\n", CPU.REG.PC);
            CPU.error = -1;
            return -1;
        }
        if (CPU.REG.PC + 3 >= CPU.ROM_SIZE + ROM_BASE)
        {
            M4_DEBUG("[ERROR] Invalid PC access: 0x%08X\n", CPU.REG.PC);
            CPU.error = -1;
            return -1;
        }
        CPU.op = (CPU.op << 16) | READ_ROM_16(CPU.REG.PC + 2);
        return m4_execute_32();
    }
    else
    {
        return m4_execute_16();
    }
}