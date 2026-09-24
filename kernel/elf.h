#ifndef ELF_H
#define ELF_H

#include "../include/types.h"
#include "paging.h"

/* ---- User virtual memory band (must match process.h USER_STACK_TOP) ---- */
#define ELF_USER_VMA_MIN 0x40000000ULL
#define ELF_USER_VMA_MAX 0xBFFFF000ULL

/* ELF32 Header */
typedef struct elf32_header {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} PACKED elf32_header_t;

/* ELF32 Program Header */
typedef struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} PACKED elf32_phdr_t;

/* ELF64 Header */
typedef struct elf64_header {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} PACKED elf64_header_t;

/* ELF64 Program Header */
typedef struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} PACKED elf64_phdr_t;

/* ELF constants */
#define ELF_MAGIC       0x464C457F   /* "\x7FELF" */
#define ELF_PT_LOAD     1
#define ELF_PF_X        0x1
#define ELF_PF_W        0x2
#define ELF_PF_R        0x4

#define ELFCLASS32      1
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EM_386          3
#define EM_X86_64       62

uint64_t elf_load(page_directory_t *page_dir, const uint8_t *data, uint64_t size);
int      elf_validate(const uint8_t *data, uint64_t size);

#endif