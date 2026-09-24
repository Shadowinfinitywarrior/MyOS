#ifndef MYLANG_H
#define MYLANG_H

#include "../include/types.h"

#define MYLANG_MAX_CODE 65536
#define MYLANG_MAX_STACK 4096
#define MYLANG_MAX_SYMBOLS 1024

typedef enum {
    OP_NOP,
    OP_PUSH,
    OP_POP,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_PRINT,
    OP_INPUT,
    OP_JMP,
    OP_JZ,
    OP_CALL,
    OP_RET,
    OP_HALT
} opcode_t;

typedef struct {
    uint8_t  code[MYLANG_MAX_CODE];
    uint32_t code_len;
    char     symbols[MYLANG_MAX_SYMBOLS][64];
    uint32_t sym_count;
} mylang_program_t;

int  mylang_compile(const char *src, mylang_program_t *out);
int  mylang_interpret(mylang_program_t *prog);
int  mylang_compose(mylang_program_t *prog, const char *outfile);

#endif
