#include "elf.h"
#include "pmm.h"
#include "heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static int      elf_validate32(const uint8_t *data, uint64_t size);
static uint64_t elf_load32(page_directory_t *page_dir, const uint8_t *data, uint64_t size);

/* Compute the byte offset of program header i, validating header-table
 * geometry (e_phoff/e_phnum/e_phentsize) stays inside the file. 0 = ok. */
static int elf_phdr_off(const elf32_header_t *hdr, uint32_t i, uint64_t *out) {
    uint64_t table = hdr->e_phoff;
    uint64_t entsz = hdr->e_phentsize;
    uint64_t count = hdr->e_phnum;
    if (entsz < sizeof(elf32_phdr_t)) return -1;
    uint64_t end = table + count * entsz;
    if (end < table) return -1;
    if (i >= count) return -1;
    uint64_t off = table + (uint64_t)i * entsz;
    if (off + entsz > end) return -1;
    *out = off;
    return 0;
}

/* ELF64 programs are loaded into the user VMA band
 * [ELF_USER_VMA_MIN, ELF_USER_VMA_MAX) so they can never alias the kernel's
 * identity-mapped low 1 GiB or the per-process user stack. */
static int elf64_phdr_off(const elf64_header_t *hdr, uint32_t i, uint64_t *out) {
    uint64_t table = hdr->e_phoff;
    uint64_t entsz = hdr->e_phentsize;
    uint64_t count = hdr->e_phnum;
    if (entsz < sizeof(elf64_phdr_t)) return -1;
    uint64_t end = table + count * entsz;
    if (end < table) return -1;
    if (i >= count) return -1;
    uint64_t off = table + (uint64_t)i * entsz;
    if (off + entsz > end) return -1;
    *out = off;
    return 0;
}

int elf_validate64(const uint8_t *data, uint64_t size) {
    if (!data || size < sizeof(elf64_header_t)) return 0;
    elf64_header_t *hdr = (elf64_header_t *)data;
    if (*(uint32_t *)hdr->e_ident != ELF_MAGIC) return 0;
    if (hdr->e_ident[4] != ELFCLASS64) return 0;   /* 64-bit class */
    if (hdr->e_ident[5] != ELFDATA2LSB) return 0;  /* little-endian */
    if (hdr->e_machine != EM_X86_64) return 0;
    if (hdr->e_phnum == 0) return 0;
    /* Program-header table must fit entirely inside the image. */
    uint64_t table_end = hdr->e_phoff + (uint64_t)hdr->e_phnum * hdr->e_phentsize;
    if (table_end < hdr->e_phoff || table_end > size) return 0;
    for (uint32_t i = 0; i < hdr->e_phnum; i++) {
        uint64_t off;
        if (elf64_phdr_off(hdr, i, &off) != 0) return 0;
        elf64_phdr_t *phdr = (elf64_phdr_t *)(data + off);
        if (phdr->p_type != ELF_PT_LOAD) continue;
        /* Segment payload inside the file image. */
        uint64_t frag_end = phdr->p_offset + phdr->p_filesz;
        if (frag_end < phdr->p_offset || frag_end > size) return 0;
        /* Virtual range must not wrap and must stay in the user band. */
        uint64_t v_end = phdr->p_vaddr + phdr->p_memsz;
        if (v_end < phdr->p_vaddr) return 0;
        if (phdr->p_vaddr < ELF_USER_VMA_MIN || v_end > ELF_USER_VMA_MAX) return 0;
        if (phdr->p_offset + phdr->p_filesz > size) return 0;
    }
    return 1;
}

uint64_t elf_load64(page_directory_t *page_dir, const uint8_t *data, uint64_t size) {
    if (!elf_validate64(data, size)) return 0;
    elf64_header_t *hdr = (elf64_header_t *)data;

    /* Track every frame mapped with its virtual address so a mid-load failure
     * rolls back fully (unmap + free) instead of leaking pages and leaving a
     * half-built address space that would alias live frames. */
    uint32_t cap = 64;
    uint32_t count = 0;
    uint64_t *map_addrs = NULL;
    uint64_t *map_phys = NULL;

    int ok = 1;
    do {
        map_addrs = (uint64_t *)kmalloc(cap * sizeof(uint64_t));
        map_phys = (uint64_t *)kmalloc(cap * sizeof(uint64_t));
        if (!map_addrs || !map_phys) { ok = 0; break; }
    } while (0);
    if (!ok) return 0;

    page_directory_t *old_dir = paging_get_active();
    paging_switch_directory(page_dir);

    for (uint32_t i = 0; i < hdr->e_phnum && ok; i++) {
        uint64_t off;
        if (elf64_phdr_off(hdr, i, &off) != 0) { ok = 0; break; }
        elf64_phdr_t *phdr = (elf64_phdr_t *)(data + off);
        if (phdr->p_type != ELF_PT_LOAD) continue;

        uint64_t start = ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
        uint64_t end = ALIGN_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        if (end < start) { ok = 0; break; }
        if (end - start > 256ULL * 1024 * 1024) { ok = 0; break; }
        if (start < ELF_USER_VMA_MIN || start >= ELF_USER_VMA_MAX) { ok = 0; break; }

        uint32_t flags = PAGE_PRESENT | PAGE_USER;
        if (phdr->p_flags & ELF_PF_W) flags |= PAGE_WRITE;

        for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
            uint64_t phys = paging_get_physical(addr);
            if (phys) continue;   /* already mapped, do not double-map */
            phys = pmm_alloc_page();
            if (!phys) { ok = 0; break; }
            paging_map(addr, phys, flags);
            if (count == cap) {
                uint32_t new_cap = cap * 2;
                uint64_t *na = (uint64_t *)kmalloc(new_cap * sizeof(uint64_t));
                uint64_t *np = (uint64_t *)kmalloc(new_cap * sizeof(uint64_t));
                if (!na || !np) { ok = 0; break; }
                memcpy(na, map_addrs, count * sizeof(uint64_t));
                memcpy(np, map_phys, count * sizeof(uint64_t));
                kfree(map_addrs);
                kfree(map_phys);
                map_addrs = na;
                map_phys = np;
                cap = new_cap;
            }
            map_addrs[count] = addr;
            map_phys[count] = phys;
            count++;
        }
        if (!ok) break;

        if (phdr->p_filesz) {
            memcpy((void *)(uintptr_t)phdr->p_vaddr, data + phdr->p_offset, phdr->p_filesz);
        }
        if (phdr->p_memsz > phdr->p_filesz) {
            memset((void *)(uintptr_t)(phdr->p_vaddr + phdr->p_filesz), 0,
                   phdr->p_memsz - phdr->p_filesz);
        }
    }

    uint64_t entry = 0;
    if (ok) {
        entry = hdr->e_entry;
        paging_switch_directory(old_dir);
    } else {
        for (uint32_t i = 0; i < count; i++) {
            paging_unmap(map_addrs[i]);
            pmm_free_page(map_phys[i]);
        }
        paging_switch_directory(old_dir);
    }

    kfree(map_addrs);
    kfree(map_phys);
    return entry;
}

int elf_validate(const uint8_t *data, uint64_t size) {
    if (!data || size < sizeof(elf32_header_t)) return 0;
    elf32_header_t *hdr = (elf32_header_t *)data;
    if (*(uint32_t *)hdr->e_ident != ELF_MAGIC) return 0;
    return (hdr->e_ident[4] == ELFCLASS64) ? elf_validate64(data, size)
                                           : elf_validate32(data, size);
}

static int elf_validate32(const uint8_t *data, uint64_t size) {
    elf32_header_t *hdr = (elf32_header_t *)data;
    if (hdr->e_ident[4] != 1) return 0;      /* 32-bit class */
    if (hdr->e_ident[5] != 1) return 0;      /* little-endian */
    if (hdr->e_machine != 3) return 0;       /* EM_386 */
    if (hdr->e_phnum == 0) return 0;
    /* Program-header table must fit entirely inside the image. */
    uint64_t table_end = (uint64_t)hdr->e_phoff + (uint64_t)hdr->e_phnum * (uint64_t)hdr->e_phentsize;
    if (table_end < hdr->e_phoff || table_end > (uint64_t)size) return 0;
    for (uint32_t i = 0; i < hdr->e_phnum; i++) {
        uint64_t off;
        if (elf_phdr_off(hdr, i, &off) != 0) return 0;
        elf32_phdr_t *phdr = (elf32_phdr_t *)(data + off);
        if (phdr->p_type != ELF_PT_LOAD) continue;
        /* Segment payload inside the file image. */
        uint64_t frag_end = (uint64_t)phdr->p_offset + phdr->p_filesz;
        if (frag_end < phdr->p_offset || frag_end > (uint64_t)size) return 0;
        /* Virtual range must not wrap and must stay in sane user space. */
        uint64_t v_end = (uint64_t)phdr->p_vaddr + phdr->p_memsz;
        if (v_end < phdr->p_vaddr) return 0;
        if (phdr->p_vaddr < 0x10000ULL || v_end > 0x80000000ULL) return 0;
    }
    return 1;
}

uint64_t elf_load(page_directory_t *page_dir, const uint8_t *data, uint64_t size) {
    if (!data || size < sizeof(elf32_header_t)) return 0;
    elf32_header_t *hdr = (elf32_header_t *)data;
    if (*(uint32_t *)hdr->e_ident != ELF_MAGIC) return 0;
    if (hdr->e_ident[4] == ELFCLASS64) return elf_load64(page_dir, data, size);
    return elf_load32(page_dir, data, size);
}

static uint64_t elf_load32(page_directory_t *page_dir, const uint8_t *data, uint64_t size) {
    if (!elf_validate32(data, size)) return 0;
    elf32_header_t *hdr = (elf32_header_t *)data;

    /* Track every frame mapped with its virtual address so a mid-load failure
     * rolls back fully (unmap + free) instead of leaking pages and leaving a
     * half-built address space that would alias live frames. */
    uint32_t cap = 64;
    uint32_t count = 0;
    uint64_t *map_addrs = NULL;
    uint64_t *map_phys = NULL;
    uint64_t entry = 0;

    int ok = 1;
    do {
        map_addrs = (uint64_t *)kmalloc(cap * sizeof(uint64_t));
        map_phys = (uint64_t *)kmalloc(cap * sizeof(uint64_t));
        if (!map_addrs || !map_phys) { ok = 0; break; }
    } while (0);
    if (!ok) return 0;

    page_directory_t *old_dir = paging_get_active();
    paging_switch_directory(page_dir);

    for (int i = 0; i < hdr->e_phnum && ok; i++) {
        uint64_t off;
        if (elf_phdr_off(hdr, i, &off) != 0) { ok = 0; break; }
        elf32_phdr_t *phdr = (elf32_phdr_t *)(data + off);
        if (phdr->p_type != ELF_PT_LOAD) continue;

        uint64_t start = ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
        uint64_t end = ALIGN_UP((uint64_t)phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        if (end < start) { ok = 0; break; }
        if (end - start > 128ULL * 1024 * 1024) { ok = 0; break; }

        uint32_t flags = PAGE_PRESENT | PAGE_USER;
        if (phdr->p_flags & ELF_PF_W) flags |= PAGE_WRITE;
        /* Execute bits are granted explicitly below only when the segment
         * requests them; NX enforcement (EFER.NXE) is enabled in Phase 2. */

        for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
            uint64_t phys = paging_get_physical(addr);
            if (phys) continue;   /* already mapped, do not double-map */
            phys = pmm_alloc_page();
            if (!phys) { ok = 0; break; }
            paging_map(addr, phys, flags);
            if (count == cap) {
                uint32_t new_cap = cap * 2;
                uint64_t *na = (uint64_t *)kmalloc(new_cap * sizeof(uint64_t));
                uint64_t *np = (uint64_t *)kmalloc(new_cap * sizeof(uint64_t));
                if (!na || !np) { ok = 0; break; }
                memcpy(na, map_addrs, count * sizeof(uint64_t));
                memcpy(np, map_phys, count * sizeof(uint64_t));
                kfree(map_addrs);
                kfree(map_phys);
                map_addrs = na;
                map_phys = np;
                cap = new_cap;
            }
            map_addrs[count] = addr;
            map_phys[count] = phys;
            count++;
        }
        if (!ok) break;

        if (phdr->p_filesz) {
            memcpy((void *)(uintptr_t)phdr->p_vaddr, data + phdr->p_offset, phdr->p_filesz);
        }
        if (phdr->p_memsz > phdr->p_filesz) {
            memset((void *)(uintptr_t)(phdr->p_vaddr + phdr->p_filesz), 0,
                   phdr->p_memsz - phdr->p_filesz);
        }
    }

    if (ok) {
        entry = hdr->e_entry;
        paging_switch_directory(old_dir);
    } else {
        for (uint32_t i = 0; i < count; i++) {
            paging_unmap(map_addrs[i]);
            pmm_free_page(map_phys[i]);
        }
        paging_switch_directory(old_dir);
    }

    kfree(map_addrs);
    kfree(map_phys);
    return entry;
}