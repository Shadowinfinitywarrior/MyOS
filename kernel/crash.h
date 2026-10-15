#ifndef CRASH_H
#define CRASH_H

#include "../include/types.h"
#include "../include/system.h"

#define CRASH_DUMP_MAGIC 0xDEADC0DE

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t timestamp;
    registers_t regs;
    uint64_t stack_trace[64];
    uint32_t stack_depth;
    char panic_msg[256];
    uint8_t kernel_memory[64 * 1024]; /* 64KB kernel state */
} crash_dump_t;

void crash_dump_save(const char *msg, registers_t *regs);
void crash_dump_check(void);

#endif
