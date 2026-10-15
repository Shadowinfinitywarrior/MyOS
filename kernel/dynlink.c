#include "dynlink.h"
#include "heap.h"
#include "aslr.h"
#include "paging.h"
#include "../fs/vfs.h"
#include "../lib/string.h"
#include "../lib/printf.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static shared_object_t *loaded_objects = NULL;
static spinlock_t dynlink_lock = SPINLOCK_INIT;

static uint64_t dynlink_find_symbol(shared_object_t *obj, const char *name) {
    if (!obj || !obj->symtab || !obj->strtab) return 0;
    for (shared_object_t *curr = loaded_objects; curr; curr = curr->next) {
        if (!curr->symtab || !curr->strtab) continue;
        for (int i = 0; i < 512; i++) {
            Elf64_Sym *sym = &curr->symtab[i];
            if (sym->st_name == 0) continue;
            const char *sym_name = &curr->strtab[sym->st_name];
            if (strcmp(sym_name, name) == 0) {
                return curr->base_addr + sym->st_value;
            }
        }
    }
    return 0;
}

static void dynlink_process_dynamic(shared_object_t *obj, void *base, elf64_phdr_t *phdrs, uint16_t phnum) {
    for (int i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            obj->dynamic = (Elf64_Dyn *)((uintptr_t)base + phdrs[i].p_vaddr);
            for (Elf64_Dyn *d = obj->dynamic; d->d_tag != DT_NULL; d++) {
                switch (d->d_tag) {
                    case DT_STRTAB:
                        obj->strtab = (const char *)((uintptr_t)base + d->d_un.d_ptr);
                        break;
                    case DT_SYMTAB:
                        obj->symtab = (Elf64_Sym *)((uintptr_t)base + d->d_un.d_ptr);
                        break;
                    case DT_RELA:
                        obj->rela = (Elf64_Rela *)((uintptr_t)base + d->d_un.d_ptr);
                        break;
                    case DT_RELASZ:
                        obj->rela_size = d->d_un.d_val;
                        break;
                    case DT_JMPREL:
                        obj->plt_rela = (Elf64_Rela *)((uintptr_t)base + d->d_un.d_ptr);
                        break;
                    case DT_PLTRELSZ:
                        obj->plt_rela_size = d->d_un.d_val;
                        break;
                }
            }
            break;
        }
    }
}

static void dynlink_resolve_rela(shared_object_t *obj) {
    if (!obj || !obj->rela) return;
    uint64_t count = obj->rela_size / sizeof(Elf64_Rela);

    for (uint64_t i = 0; i < count; i++) {
        uint64_t *target = (uint64_t *)(uintptr_t)(obj->base_addr + obj->rela[i].r_offset);
        uint32_t type = ELF64_R_TYPE(obj->rela[i].r_info);
        uint32_t sym_idx = ELF64_R_SYM(obj->rela[i].r_info);

        switch (type) {
            case R_X86_64_RELATIVE:
                *target = obj->base_addr + obj->rela[i].r_addend;
                break;
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT: {
                if (obj->symtab && obj->strtab) {
                    Elf64_Sym *sym = &obj->symtab[sym_idx];
                    uint64_t addr = dynlink_find_symbol(obj, &obj->strtab[sym->st_name]);
                    if (addr) *target = addr;
                }
                break;
            }
            case R_X86_64_64: {
                if (obj->symtab && obj->strtab) {
                    Elf64_Sym *sym = &obj->symtab[sym_idx];
                    uint64_t addr = dynlink_find_symbol(obj, &obj->strtab[sym->st_name]);
                    if (addr) *target = addr + obj->rela[i].r_addend;
                }
                break;
            }
        }
    }
}

int dynlink_load(const char *path, process_t *proc) {
    (void)proc;
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) return -1;

    elf64_header_t ehdr;
    if (vfs_read(node, 0, sizeof(ehdr), &ehdr) != (int)sizeof(ehdr)) return -1;
    if (*(uint32_t *)ehdr.e_ident != ELF_MAGIC || ehdr.e_ident[4] != ELFCLASS64) return -1;

    elf64_phdr_t *phdrs = (elf64_phdr_t *)kmalloc(ehdr.e_phnum * sizeof(elf64_phdr_t));
    if (!phdrs) return -1;
    vfs_read(node, ehdr.e_phoff, ehdr.e_phnum * sizeof(elf64_phdr_t), phdrs);

    uint64_t min_vaddr = (uint64_t)-1, max_vaddr = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == ELF_PT_LOAD) {
            if (phdrs[i].p_vaddr < min_vaddr) min_vaddr = phdrs[i].p_vaddr;
            if (phdrs[i].p_vaddr + phdrs[i].p_memsz > max_vaddr)
                max_vaddr = phdrs[i].p_vaddr + phdrs[i].p_memsz;
        }
    }

    uint64_t load_bias = aslr_mmap_base();
    uint64_t map_size = (max_vaddr - min_vaddr + 4095) & ~4095ULL;
    void *base = (void *)(uintptr_t)load_bias;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == ELF_PT_LOAD) {
            void *dest = (void *)((uintptr_t)base + phdrs[i].p_vaddr);
            vfs_read(node, phdrs[i].p_offset, phdrs[i].p_filesz, dest);
            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                memset((void *)((uintptr_t)dest + phdrs[i].p_filesz), 0,
                       phdrs[i].p_memsz - phdrs[i].p_filesz);
            }
        }
    }

    shared_object_t *obj = (shared_object_t *)kmalloc(sizeof(shared_object_t));
    if (!obj) {
        kfree(phdrs);
        return -1;
    }
    memset(obj, 0, sizeof(shared_object_t));
    obj->base_addr = (uint64_t)base;
    obj->entry = (uint64_t)base + ehdr.e_entry;
    obj->phdr = phdrs;
    obj->phnum = ehdr.e_phnum;
    strncpy(obj->name, path, sizeof(obj->name) - 1);

    dynlink_process_dynamic(obj, base, phdrs, ehdr.e_phnum);
    dynlink_resolve_rela(obj);

    uint64_t flags = spin_lock_irqsave(&dynlink_lock);
    obj->next = loaded_objects;
    loaded_objects = obj;
    spin_unlock_irqrestore(&dynlink_lock, flags);

    kprintf("[DYNLINK] Loaded shared library %s at base 0x%lx (size %lu)\n",
            path, (unsigned long)obj->base_addr, (unsigned long)map_size);
    return 0;
}

int elf_dynlink_load(const char *path) {
    return dynlink_load(path, process_get_current());
}

void *dlopen(const char *path) {
    if (!path) return (void *)1;
    uint64_t flags = spin_lock_irqsave(&dynlink_lock);
    for (shared_object_t *curr = loaded_objects; curr; curr = curr->next) {
        if (strcmp(curr->name, path) == 0) {
            spin_unlock_irqrestore(&dynlink_lock, flags);
            return (void *)curr;
        }
    }
    spin_unlock_irqrestore(&dynlink_lock, flags);

    if (dynlink_load(path, process_get_current()) == 0) {
        return (void *)loaded_objects;
    }
    return NULL;
}

void *dlsym(void *handle, const char *symbol) {
    if (!symbol) return NULL;
    shared_object_t *obj = (handle == (void *)1) ? loaded_objects : (shared_object_t *)handle;
    uint64_t addr = dynlink_find_symbol(obj, symbol);
    return (void *)(uintptr_t)addr;
}

int dlclose(void *handle) {
    (void)handle;
    return 0;
}
