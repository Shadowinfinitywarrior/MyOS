#ifndef SYSTEM_H
#define SYSTEM_H

#include "types.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret; __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret; __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t ret; __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void io_wait(void) { outb(0x80, 0); }

static inline void cli(void) { __asm__ __volatile__("cli"); }
static inline void sti(void) { __asm__ __volatile__("sti"); }
static inline void hlt(void) { __asm__ __volatile__("hlt"); }

static inline void stac(void) {
    uint64_t bits = 1ULL << 18;
    __asm__ __volatile__("pushfq\n\t"
                         "orq %0, 8(%%rsp)\n\t"
                         "popfq"
                         : : "r"(bits) : "memory", "cc");
}
static inline void clac(void) {
    uint64_t bits = ~(1ULL << 18);
    __asm__ __volatile__("pushfq\n\t"
                         "andq %0, 8(%%rsp)\n\t"
                         "popfq"
                         : : "r"(bits) : "memory", "cc");
}

static inline uint64_t read_cr0(void) { uint64_t v; __asm__ __volatile__("mov %%cr0, %0":"=r"(v)); return v; }
static inline uint64_t read_cr2(void) { uint64_t v; __asm__ __volatile__("mov %%cr2, %0":"=r"(v)); return v; }
static inline uint64_t read_cr3(void) { uint64_t v; __asm__ __volatile__("mov %%cr3, %0":"=r"(v)); return v; }
static inline void write_cr0(uint64_t v){ __asm__ __volatile__("mov %0, %%cr0"::"r"(v)); }
static inline void write_cr3(uint64_t v){ __asm__ __volatile__("mov %0, %%cr3"::"r"(v)); }
static inline void invlpg(uint64_t addr){ __asm__ __volatile__("invlpg (%0)"::"r"(addr):"memory"); }

static inline uint64_t read_eflags(void){
    uint64_t e; __asm__ __volatile__("pushfq\npopq %0":"=r"(e)); return e;
}

static inline void write_eflags(uint64_t e){
    __asm__ __volatile__("pushq %0\npopfq" : : "r"(e) : "memory");
}

static inline NORETURN void hang(void){ cli(); for(;;) hlt(); __builtin_unreachable(); }

typedef struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} PACKED registers_t;

typedef void (*isr_handler_t)(registers_t *);

void kernel_panic(const char *message, const char *file, int line);
#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)
#define ASSERT(cond) do { if(!(cond)) PANIC("Assertion failed: " #cond); } while(0)

#endif
