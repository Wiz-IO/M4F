#include "M4.h"
#include "common.h"

CortexM4 CPU;

///////////////////////////////////////////////////////////

uint32_t READ_MEM_32(uint32_t address, int *result)
{
    if (!result)
    {
        PRINTF("[ERROR] READ_MEM_32: Invalid Parameter\n");
        exit(0);
    }
    *result = 0;
    uint32_t offset;
    if (address >= ROM_BASE && address < ROM_BASE + CPU.ROM_SIZE)
    {
        // Четене от ROM
        offset = address - ROM_BASE;
        if (offset + 3 < CPU.ROM_SIZE)
        {
            return ((uint32_t)CPU.ROM[offset] |
                    ((uint32_t)CPU.ROM[offset + 1] << 8) |
                    ((uint32_t)CPU.ROM[offset + 2] << 16) |
                    ((uint32_t)CPU.ROM[offset + 3] << 24));
        }
    }
    else if (address >= RAM_BASE && address < RAM_BASE + CPU.RAM_SIZE)
    {
        // Четене от RAM
        offset = address - RAM_BASE;
        if (offset + 3 < CPU.RAM_SIZE)
        {
            return ((uint32_t)CPU.RAM[offset] |
                    ((uint32_t)CPU.RAM[offset + 1] << 8) |
                    ((uint32_t)CPU.RAM[offset + 2] << 16) |
                    ((uint32_t)CPU.RAM[offset + 3] << 24));
        }
    }
    // Невалиден адрес
    PRINTF("[ERROR] READ_MEM_32: Invalid Address: 0x%08X\n", address);
    *result = -1;
    return 0; // Връща 0 при невалиден достъп
}

uint16_t READ_MEM_16(uint32_t address, int *result)
{
    if (!result)
    {
        PRINTF("[ERROR] READ_MEM: Invalid Parameter\n");
        exit(0);
    }
    *result = 0;
    uint32_t offset;
    if (address >= ROM_BASE && address < ROM_BASE + CPU.ROM_SIZE)
    {
        // Четене от ROM
        offset = address - ROM_BASE;
        if (offset + 1 < CPU.ROM_SIZE)
        {
            return ((uint16_t)CPU.ROM[offset] | ((uint16_t)CPU.ROM[offset + 1] << 8));
        }
    }
    else if (address >= RAM_BASE && address < RAM_BASE + CPU.RAM_SIZE)
    {
        // Четене от RAM
        offset = address - RAM_BASE;
        if (offset + 1 < CPU.RAM_SIZE)
        {
            return ((uint16_t)CPU.RAM[offset] | ((uint16_t)CPU.RAM[offset + 1] << 8));
        }
    }
    // Невалиден адрес или размер
    PRINTF("[ERROR] READ_MEM_16: Invalid Address: 0x%08X\n", address);
    *result = -1; // Връща -1 при невалиден достъп
    return 0;     // без значение
}

uint8_t READ_MEM_8(uint32_t address, int *result)
{
    if (!result)
    {
        PRINTF("[ERROR] READ_MEM: Invalid Parameter\n");
        exit(0);
    }
    *result = 0;
    uint32_t offset;
    if (address >= ROM_BASE && address < ROM_BASE + CPU.ROM_SIZE)
    {
        // Четене от ROM
        offset = address - ROM_BASE;
        if (offset < CPU.ROM_SIZE)
            return CPU.ROM[offset];
    }
    else if (address >= RAM_BASE && address < RAM_BASE + CPU.RAM_SIZE)
    {
        // Четене от RAM
        offset = address - RAM_BASE;
        if (offset < CPU.RAM_SIZE)
            return CPU.RAM[offset];
    }
    // Невалиден адрес или размер
    PRINTF("[ERROR] READ_MEM_8: Invalid Address: 0x%08X\n", address);
    *result = -1; // Връща -1 при невалиден достъп
    return 0;     // без значение
}

uint32_t READ_THUMB_32(uint32_t address, int *result){
    if (!result)
    {
        PRINTF("[ERROR] READ_MEM_32: Invalid Parameter\n");
        exit(0);
    }
    *result = 0;
    uint32_t offset;
    if (address >= ROM_BASE && address < ROM_BASE + CPU.ROM_SIZE)
    {
        // Четене от ROM
        offset = address - ROM_BASE;
        if (offset + 3 < CPU.ROM_SIZE)
        {
            return CPU.ROM[offset+2] | (CPU.ROM[offset+3] << 8) | (CPU.ROM[offset+0] << 16) | (CPU.ROM[offset+1] << 24);
        }
    }
}

///////////////////////////////////////////////////////////

int WRITE_MEM_32(uint32_t address, uint32_t data)
{
    // Проверка дали адресът е в обхвата на RAM
    if (address >= RAM_BASE && address < RAM_BASE + CPU.RAM_SIZE)
    {
        uint32_t offset = address - RAM_BASE;
        // Проверка дали има достатъчно място за 4 байта
        if (offset + 3 < CPU.RAM_SIZE)
        {
            CPU.RAM[offset] = (uint8_t)(data & 0xFF);
            CPU.RAM[offset + 1] = (uint8_t)((data >> 8) & 0xFF);
            CPU.RAM[offset + 2] = (uint8_t)((data >> 16) & 0xFF);
            CPU.RAM[offset + 3] = (uint8_t)((data >> 24) & 0xFF);
            return 0; // Успешен запис
        }
    }
    PRINTF("[ERROR] WRITE_MEM_32: Invalid Address: 0x%08X, Data: 0x%08X\n", address, data);
    return -1; // Невалиден адрес или недостатъчно място
}

int WRITE_MEM_16(uint32_t address, uint16_t data)
{
    // Проверка дали адресът е в обхвата на RAM
    if (address >= RAM_BASE && address < RAM_BASE + CPU.RAM_SIZE)
    {
        uint32_t offset = address - RAM_BASE;
        // Проверка дали има достатъчно място за 4 байта
        if (offset + 1 < CPU.RAM_SIZE)
        {
            CPU.RAM[offset] = (uint8_t)(data & 0xFF);
            CPU.RAM[offset + 1] = (uint8_t)(data >> 8);
            return 0; // Успешен запис
        }
    }
    PRINTF("[ERROR] WRITE_MEM_16: Invalid Address: 0x%08X, Data: 0x%08X\n", address, data);
    return -1; // Невалиден адрес или недостатъчно място
}

int WRITE_MEM_8(uint32_t address, uint8_t data)
{
    // Проверка дали адресът е в обхвата на RAM
    if (address >= RAM_BASE && address < RAM_BASE + CPU.RAM_SIZE)
    {
        uint32_t offset = address - RAM_BASE;
        // Проверка дали има достатъчно място за 4 байта
        if (offset < CPU.RAM_SIZE)
        {
            CPU.RAM[offset] = data;
            return 0; // Успешен запис
        }
    }
    PRINTF("[ERROR] WRITE_MEM_8: Invalid Address: 0x%08X, Data: 0x%08X\n", address, data);
    return -1; // Невалиден адрес или недостатъчно място
}

///////////////////////////////////////////////////////////

void PRINT_REG(void)
{
    printf("=== CPU Registers ===\n");
    for (int i = 0; i < 16; i++)
    {
        const char *reg_name;
        switch (i)
        {
        case 13:
            reg_name = "SP ";
            break;
        case 14:
            reg_name = "LR ";
            break;
        case 15:
            reg_name = "PC ";
            break;
        default:
        {
            static char buf[8];
            snprintf(buf, sizeof(buf), "R%02d", i);
            reg_name = buf;
            break;
        }
        }
        printf("%s = 0x%08X\n", reg_name, CPU.REG.r[i]);
    }
    printf("===================\n");
}

///////////////////////////////////////////////////////////

int m4_execute(void)
{
    FUNC_VM();

    if (CPU.REG.PC & 0x1)
    {
        DEBUG_M4("[ERROR] Unaligned PC: 0x%08X\n", CPU.REG.PC);
        RETURN_ERROR(-1);
    }

    if (!CPU.psr.epsr.T)
    {
        DEBUG_M4("[ERROR] Invalid Thumb state at PC: 0x%08X\n", CPU.REG.PC);
        RETURN_ERROR(-1);
    }

    int res;
    CPU.op = READ_MEM_16(CPU.REG.PC, &res);
    if (res) // Проверка за граници, има съобщение за грешка
    {
        RETURN_ERROR(-1);
    }

    if ((CPU.op & 0xF800) >= 0xE800)
    {
        if (CPU.REG.PC & 0x3)
        {
            DEBUG_M4("[ERROR] Unaligned PC for 32-bit instruction: 0x%08X\n", CPU.REG.PC);
            RETURN_ERROR(-1);
        }
        if (CPU.REG.PC + 3 >= CPU.ROM_SIZE + ROM_BASE)
        {
            DEBUG_M4("[ERROR] Invalid PC access: 0x%08X\n", CPU.REG.PC);
            RETURN_ERROR(-1);
        }

        CPU.op = READ_THUMB_32(CPU.REG.PC, &res);
        if (res) // Проверка за граници, има съобщение за грешка
        {
            RETURN_ERROR(-1);
        }

        return m4_execute_32();
    }
    else
    {
        return m4_execute_16();
    }
}

///////////////////////////////////////////////////////////