#include "mylang.h"
#include "../drivers/screen.h"
#include "../lib/printf.h"

int mylang_interpret(mylang_program_t *prog){
    if(!prog) return -1;
    int32_t stack[MYLANG_MAX_STACK];
    int sp=0;
    uint32_t pc=0;
    while(pc < prog->code_len){
        uint8_t op = prog->code[pc++];
        switch(op){
            case OP_NOP: break;
            case OP_PUSH:{
                int32_t v = prog->code[pc] | (prog->code[pc+1]<<8) | (prog->code[pc+2]<<16) | (prog->code[pc+3]<<24);
                pc+=4;
                stack[sp++]=v;
                break;
            }
            case OP_ADD:{
                int32_t b=stack[--sp];
                int32_t a=stack[--sp];
                stack[sp++]=a+b;
                break;
            }
            case OP_PRINT:{
                int32_t v=stack[--sp];
                char buf[32];
                itoa(v,buf,10);
                screen_write(buf);
                screen_write("\n");
                break;
            }
            case OP_HALT:
                return 0;
            default:
                break;
        }
    }
    return 0;
}
