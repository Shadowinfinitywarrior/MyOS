#ifndef DYNLINK_H
#define DYNLINK_H

#include "../include/types.h"
#include "elf.h"
#include "process.h"

#define PT_DYNAMIC 2

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_PLTGOT   3
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_INIT     12
#define DT_FINI     13
#define DT_JMPREL   23
#define DT_INIT_ARRAY 25
#define DT_INIT_ARRAYSZ 27

#define R_X86_64_NONE      0
#define R_X86_64_64        1
#define R_X86_64_COPY      5
#define R_X86_64_GLOB_DAT  6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE  8

#define ELF64_R_SYM(i)     ((i) >> 32)
#define ELF64_R_TYPE(i)    ((i) & 0xFFFFFFFFL)

typedef struct {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} PACKED Elf64_Dyn;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} PACKED Elf64_Rela;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} PACKED Elf64_Sym;

typedef struct shared_object {
    uint64_t base_addr;
    uint64_t entry;
    elf64_phdr_t *phdr;
    uint16_t phnum;
    Elf64_Dyn *dynamic;
    Elf64_Sym *symtab;
    const char *strtab;
    uint32_t *hash;
    Elf64_Rela *rela;
    uint64_t rela_size;
    Elf64_Rela *plt_rela;
    uint64_t plt_rela_size;
    char name[256];
    struct shared_object *next;
} shared_object_t;

int elf_dynlink_load(const char *path);
int dynlink_load(const char *path, process_t *proc);
void *dlopen(const char *path);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);

#endif
