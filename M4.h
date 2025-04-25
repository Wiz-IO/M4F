#ifndef _M4_H_
#define _M4_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define ROM_BASE 0x00000000
#define RAM_BASE 0x20000000

#define USE_FPU 0
#define USE_DSP 0
#define USE_NVIC 0
#define USE_SYSTEM 0

#ifdef DISABLE_DEBUG
#define M4_DEBUG(FORMAT, ...) ((void)0) // Няма код
#else
#define M4_DEBUG(FORMAT, ...) M4_DEBUG(FORMAT, __VA_ARGS__)
#endif


/* --- 8-bit (1 bytes) --- */
#define READ_ROM_8(ADDR) (CPU.ROM[(ADDR) - ROM_BASE])
#define READ_RAM_8(ADDR) (CPU.RAM[(ADDR) - RAM_BASE])
#define WRITE_RAM_8(ADDR, DATA) (CPU.RAM[(ADDR) - RAM_BASE] = (DATA))

/* --- 16-bit (2 bytes) --- */
#define READ_ROM_16(ADDR)                   \
    ((uint16_t)CPU.ROM[(ADDR) - ROM_BASE] | \
     (uint16_t)(CPU.ROM[(ADDR) - ROM_BASE + 1] << 8))

#define READ_RAM_16(ADDR)                   \
    ((uint16_t)CPU.RAM[(ADDR) - RAM_BASE] | \
     (uint16_t)(CPU.RAM[(ADDR) - RAM_BASE + 1] << 8))

#define WRITE_RAM_16(ADDR, DATA)                                          \
    do                                                                    \
    {                                                                     \
        CPU.RAM[(ADDR) - RAM_BASE] = (uint8_t)((DATA) & 0xFF);            \
        CPU.RAM[(ADDR) - RAM_BASE + 1] = (uint8_t)(((DATA) >> 8) & 0xFF); \
    } while (0)

/* --- 32-bit (4 bytes) --- */
#define READ_ROM_32(ADDR)                               \
    ((uint32_t)CPU.ROM[(ADDR) - ROM_BASE] |             \
     (uint32_t)(CPU.ROM[(ADDR) - ROM_BASE + 1] << 8) |  \
     (uint32_t)(CPU.ROM[(ADDR) - ROM_BASE + 2] << 16) | \
     (uint32_t)(CPU.ROM[(ADDR) - ROM_BASE + 3] << 24))

#define READ_RAM_32(ADDR)                               \
    ((uint32_t)CPU.RAM[(ADDR) - RAM_BASE] |             \
     (uint32_t)(CPU.RAM[(ADDR) - RAM_BASE + 1] << 8) |  \
     (uint32_t)(CPU.RAM[(ADDR) - RAM_BASE + 2] << 16) | \
     (uint32_t)(CPU.RAM[(ADDR) - RAM_BASE + 3] << 24))

#define WRITE_RAM_32(ADDR, DATA)                                           \
    do                                                                     \
    {                                                                      \
        CPU.RAM[(ADDR) - RAM_BASE] = (uint8_t)((DATA) & 0xFF);             \
        CPU.RAM[(ADDR) - RAM_BASE + 1] = (uint8_t)(((DATA) >> 8) & 0xFF);  \
        CPU.RAM[(ADDR) - RAM_BASE + 2] = (uint8_t)(((DATA) >> 16) & 0xFF); \
        CPU.RAM[(ADDR) - RAM_BASE + 3] = (uint8_t)(((DATA) >> 24) & 0xFF); \
    } while (0)

typedef union M4_u
{
    uint32_t r[16];
    struct
    {
        uint32_t R[13];
        uint32_t SP;
        uint32_t LR;
        uint32_t PC;
    };
} M4;

#if USE_FPU
typedef struct
{
    float S[32];
    uint32_t FPSCR;
} FPU;
#endif

typedef struct
{
    uint32_t N : 1;
    uint32_t Z : 1;
    uint32_t C : 1;
    uint32_t V : 1;
#if USE_DSP
    uint32_t Q : 1;
    uint32_t reserved1 : 7;
    uint32_t GE : 4;
#else
    uint32_t reserved1 : 12;
#endif
    uint32_t reserved2 : 16;
} APSR;

#if USE_NVIC
typedef struct
{
    uint32_t ExceptionNumber : 9;
    uint32_t reserved : 23;
} IPSR;
#endif

typedef struct
{
    uint32_t T : 1;
    uint32_t ICI_IT_high : 2;
    uint32_t reserved1 : 8;
    uint32_t ICI_IT_low : 6;
    uint32_t reserved2 : 10;
} EPSR;

typedef union
{
    uint32_t value;
    APSR apsr;
#if USE_NVIC
    IPSR ipsr;
#endif
    EPSR epsr;
    struct
    {
#if USE_NVIC
        uint32_t ExceptionNumber : 9;
        uint32_t reserved1 : 1;
#else
        uint32_t reserved0 : 10;
#endif
        uint32_t ICI_IT_low : 6;
#if USE_DSP
        uint32_t GE : 4;
        uint32_t reserved2 : 4;
#else
        uint32_t reserved2 : 8;
#endif
        uint32_t T : 1;
        uint32_t ICI_IT_high : 2;
#if USE_DSP
        uint32_t Q : 1;
#else
        uint32_t reserved3 : 1;
#endif
        uint32_t V : 1;
        uint32_t C : 1;
        uint32_t Z : 1;
        uint32_t N : 1;
    };
} PSR;

#if USE_NVIC
typedef struct
{
    uint32_t ISER[8];
    uint32_t ICER[8];
    uint32_t ISPR[8];
    uint32_t ICPR[8];
    uint32_t IABR[8];
    uint8_t IPR[240];
} NVIC;
#endif

typedef struct
{
    M4 REG;
#if USE_FPU
    FPU fpu;
#endif
    PSR psr;
#if USE_NVIC
    NVIC nvic;
#endif
#if USE_SYSTEM
    uint32_t CONTROL;
    uint32_t PRIMASK;
    uint32_t FAULTMASK;
    uint32_t BASEPRI;
#endif
#if USE_NVIC
    uint32_t *vector_table;
    uint32_t vector_table_size;
#endif
    uint32_t op;
    int error;
#if 1
    FILE *file;
#endif
    const uint8_t *ROM;
    uint32_t ROM_SIZE;
    uint8_t *RAM;
    uint32_t RAM_SIZE;
} CortexM4;
extern CortexM4 CPU;

#define UPDATE_N 0x1
#define UPDATE_Z 0x2
#define UPDATE_C 0x4
#define UPDATE_V 0x8
#define UPDATE_Q 0x10
#define UPDATE_GE 0x20

typedef enum
{
    OP_ADD,
    OP_SUB,
    OP_ADC,
    OP_SBC,
    OP_RSB,
    OP_MUL,
    OP_MLA,
    OP_MLS,
    OP_LSL,
    OP_LSR,
    OP_ASR,
    OP_ROR,
    OP_AND,
    OP_ORR,
    OP_EOR,
    OP_BIC,
    OP_ORN,
    OP_MVN,
    OP_MOV,
    OP_CMP,
    OP_CMN,
    OP_TST,
    OP_TEQ,
    OP_SSAT,
    OP_USAT,
    OP_SADD16,
    OP_UADD16
} OP_TYPE;

void m4_update_apsr(uint32_t result, uint32_t op1, uint32_t op2, int operation_type, int shift_amount, int update_flags);
int m4_execute_16(void);
int m4_execute_32(void);
int m4_execute(void);

#endif // _M4_H_