#include "paging.h"
#include "pmm.h"
#include "../lib/string.h"
#include "../lib/serial_printf.h"
#include "../lib/printf.h"

#define PAGE_SIZE 4096
#define PTE_PRESENT 0x001ULL
#define PTE_WRITE   0x002ULL
#define PTE_USER    0x004ULL
#define PTE_PS      0x080ULL

#define PML4_IDX(v) (((v) >> 39) & 0x1FFULL)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FFULL)
#define PD_IDX(v)   (((v) >> 21) & 0x1FFULL)
#define PT_IDX(v)   (((v) >> 12) & 0x1FFULL)

#define PAGING_MAX_DIRS 256
static page_directory_t kernel_dir;
static page_directory_t dir_pool[PAGING_MAX_DIRS];
static page_directory_t *dir_free_list;
static page_directory_t *dir_live_list;
static uint64_t kernel_pml4_phys = 0;
static page_directory_t *active_dir = NULL;

static inline void write_cr3(uint64_t v) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(v) : "memory");
}
static inline void invlpg(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

void paging_init(void) {
    active_dir = &kernel_dir;
    dir_free_list = NULL;
    dir_live_list = NULL;
    memset(dir_pool, 0, sizeof(dir_pool));
    for (int i = PAGING_MAX_DIRS - 1; i >= 0; i--) {
        dir_pool[i].pml4_phys = 0;
        dir_pool[i].next = dir_free_list;
        dir_free_list = &dir_pool[i];
    }
    kprintf("[PAGING] active_dir set to %p\n", (void*)active_dir);
    kprintf("paging_init: start\n");
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
        kprintf("[PAGING] Failed to allocate PML4\n");
        return;
    }
    kernel_pml4_phys = phys;
    kernel_dir.pml4_phys = phys;
    kprintf("paging_init: memset phys\n");
    memset((void*)(uintptr_t)phys, 0, PAGE_SIZE);
    uint64_t *pml4 = (uint64_t*)(uintptr_t)phys;
    
    kprintf("paging_init: alloc pdpt\n");
    uint64_t pdpt_phys = pmm_alloc_page();
    memset((void*)(uintptr_t)pdpt_phys, 0, PAGE_SIZE);
    uint64_t *pdpt = (uint64_t*)(uintptr_t)pdpt_phys;

    kprintf("paging_init: alloc pd\n");
    uint64_t pd_phys = pmm_alloc_page();
    memset((void*)(uintptr_t)pd_phys, 0, PAGE_SIZE);
    uint64_t *pd = (uint64_t*)(uintptr_t)pd_phys;

    kprintf("paging_init: loop\n");
    for (int i = 0; i < 512; i++) {
        pd[i] = ((uint64_t)i << 21) | PTE_PRESENT | PTE_WRITE | PTE_PS;
    }

    pdpt[0] = pd_phys | PTE_PRESENT | PTE_WRITE;
    pml4[0] = pdpt_phys | PTE_PRESENT | PTE_WRITE;

    kprintf("paging_init: alloc pdpt_high\n");
    uint64_t pdpt_high_phys = pmm_alloc_page();
    memset((void*)(uintptr_t)pdpt_high_phys, 0, PAGE_SIZE);
    uint64_t *pdpt_high = (uint64_t*)(uintptr_t)pdpt_high_phys;
    
    pdpt_high[510] = pd_phys | PTE_PRESENT | PTE_WRITE;
    pml4[511] = pdpt_high_phys | PTE_PRESENT | PTE_WRITE;

    kprintf("paging_init: write_cr3\n");
    write_cr3(phys);
    kprintf("paging_init: done\n");
    kprintf("[PAGING] Initialized 4-level tables, PML4=0x%08lX\n", (unsigned long)phys);
    kprintf("paging enabled\n");
    kprintf("higher-half kernel\n");
}

static page_directory_t *current_dir(void) {
    return active_dir;
}

static uint64_t get_pml4_phys(page_directory_t *dir) {
    if (!dir) return kernel_pml4_phys;
    return dir->pml4_phys ? dir->pml4_phys : kernel_pml4_phys;
}

void paging_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t vaddr = (uint64_t)virt;
    uint64_t paddr = (uint64_t)phys;
    /* User pages must be reachable through every level of the walk: on
     * x86-64 a supervisor (U/S=0) non-leaf entry denies user access to the
     * entire subtree, so propagate USER up to PML4/PDPT/PD. */
    uint64_t usr = (flags & PAGE_USER) ? PTE_USER : 0;
    uint64_t pml4_phys = get_pml4_phys(current_dir());
    uint64_t *pml4 = (uint64_t*)(uintptr_t)pml4_phys;

    uint64_t i4 = PML4_IDX(vaddr);
    uint64_t i3 = PDPT_IDX(vaddr);
    uint64_t i2 = PD_IDX(vaddr);
    uint64_t i1 = PT_IDX(vaddr);

    uint64_t pdpt_phys = pml4[i4] & ~0xFFFULL;
    if (!pdpt_phys) {
        uint64_t np = pmm_alloc_page();
        if (!np) return;
        memset((void*)(uintptr_t)np, 0, PAGE_SIZE);
        pml4[i4] = ((uint64_t)np) | PTE_PRESENT | PTE_WRITE | usr;
        pdpt_phys = np;
    } else if (usr && !(pml4[i4] & PTE_USER)) {
        pml4[i4] |= PTE_USER;
    }
    uint64_t *pdpt = (uint64_t*)(uintptr_t)pdpt_phys;
    uint64_t pd_phys = pdpt[i3] & ~0xFFFULL;
    if (!pd_phys) {
        uint64_t np = pmm_alloc_page();
        if (!np) return;
        memset((void*)(uintptr_t)np, 0, PAGE_SIZE);
        pdpt[i3] = ((uint64_t)np) | PTE_PRESENT | PTE_WRITE | usr;
        pd_phys = np;
    } else if (usr && !(pdpt[i3] & PTE_USER)) {
        pdpt[i3] |= PTE_USER;
    }
    uint64_t *pd = (uint64_t*)(uintptr_t)pd_phys;
    uint64_t pde = pd[i2];
    uint64_t pt_phys;

    if (pde & PTE_PS) {
        uint64_t huge_base = pde & ~0x1FFFFFULL;
        uint64_t lflags = pde & 0xFFFULL;
        pt_phys = pmm_alloc_page();
        if (!pt_phys) return;
        memset((void*)(uintptr_t)pt_phys, 0, PAGE_SIZE);
        uint64_t *pt = (uint64_t*)(uintptr_t)pt_phys;
        for (int k = 0; k < 512; k++) {
            pt[k] = (huge_base + ((uint64_t)k << 12)) |
                    (lflags & ~PTE_PS) | PTE_PRESENT;
        }
        pd[i2] = ((uint64_t)pt_phys) | PTE_PRESENT | PTE_WRITE | usr;
    } else {
        pt_phys = pde & ~0xFFFULL;
        if (!pt_phys) {
            uint64_t np = pmm_alloc_page();
            if (!np) return;
            memset((void*)(uintptr_t)np, 0, PAGE_SIZE);
            pd[i2] = ((uint64_t)np) | PTE_PRESENT | PTE_WRITE | usr;
            pt_phys = np;
        } else if (usr && !(pd[i2] & PTE_USER)) {
            pd[i2] |= PTE_USER;
        }
    }
    uint64_t *pt = (uint64_t*)(uintptr_t)pt_phys;
    uint64_t entry = (paddr & ~0xFFFULL) | PTE_PRESENT;
    if (flags & PAGE_WRITE) entry |= PTE_WRITE;
    if (flags & PAGE_USER)  entry |= PTE_USER;
    if (flags & PAGE_NX)    entry |= (1ULL << 63);
    pt[i1] = entry;
    invlpg(vaddr);
}

void paging_unmap(uint64_t virt) {
    uint64_t vaddr = (uint64_t)virt;
    uint64_t pml4_phys = get_pml4_phys(current_dir());
    uint64_t *pml4 = (uint64_t*)(uintptr_t)pml4_phys;
    uint64_t i4 = PML4_IDX(vaddr);
    uint64_t i3 = PDPT_IDX(vaddr);
    uint64_t i2 = PD_IDX(vaddr);
    uint64_t i1 = PT_IDX(vaddr);
    uint64_t pdpt_phys = pml4[i4] & ~0xFFFULL;
    if (!pdpt_phys) return;
    uint64_t *pdpt = (uint64_t*)(uintptr_t)pdpt_phys;
    uint64_t pd_phys = pdpt[i3] & ~0xFFFULL;
    if (!pd_phys) return;
    uint64_t *pd = (uint64_t*)(uintptr_t)pd_phys;
    uint64_t pde = pd[i2];
    if (pde & PTE_PS) return;   /* 2MB huge page: don't clobber its base */
    uint64_t pt_phys = pde & ~0xFFFULL;
    if (!pt_phys) return;
    uint64_t *pt = (uint64_t*)(uintptr_t)pt_phys;
    pt[i1] = 0;
    invlpg(vaddr);
}

uint64_t paging_get_physical(uint64_t virt) {
    uint64_t vaddr = virt;
    uint64_t pml4_phys = get_pml4_phys(current_dir());
    uint64_t *pml4 = (uint64_t*)(uintptr_t)pml4_phys;
    uint64_t i4 = PML4_IDX(vaddr);
    uint64_t i3 = PDPT_IDX(vaddr);
    uint64_t i2 = PD_IDX(vaddr);
    uint64_t i1 = PT_IDX(vaddr);
    uint64_t pdpt_phys = pml4[i4] & ~0xFFFULL;
    if (!pdpt_phys) return 0;
    uint64_t *pdpt = (uint64_t*)(uintptr_t)pdpt_phys;
    uint64_t pd_phys = pdpt[i3] & ~0xFFFULL;
    if (!pd_phys) return 0;
    uint64_t *pd = (uint64_t*)(uintptr_t)pd_phys;
    uint64_t pde = pd[i2];
    if (pde & 0x80ULL) { /* 2MB huge page */
        uint64_t base = pde & ~(0x1FFFFFULL | (1ULL << 63));
        return base | (vaddr & 0x1FFFFFULL);
    }
    uint64_t pt_phys = pd[i2] & ~0xFFFULL;
    if (!pt_phys) return 0;
    uint64_t *pt = (uint64_t*)(uintptr_t)pt_phys;
    uint64_t pte = pt[i1];
    if (!(pte & PTE_PRESENT)) return 0;
    /* Mask attributes INCLUDING bit 63 (NX): the returned value must be a
     * plain physical address, never carrying page permission bits. */
    uint64_t base = pte & ~(0xFFFULL | (1ULL << 63));
    return base | (vaddr & 0xFFFULL);
}

/* Return the leaf page-table attributes (low 12 bits + NX bit 63) at virt,
 * or 0 if the page is not mapped. Used by process_fork to reproduce the
 * parent's exact permissions (exec/NX/ro/rw) on copied child frames. */
uint64_t paging_get_attrs(uint64_t virt) {
    uint64_t vaddr = virt;
    uint64_t pml4_phys = get_pml4_phys(current_dir());
    uint64_t *pml4 = (uint64_t*)(uintptr_t)pml4_phys;
    uint64_t i4 = PML4_IDX(vaddr);
    uint64_t i3 = PDPT_IDX(vaddr);
    uint64_t i2 = PD_IDX(vaddr);
    uint64_t i1 = PT_IDX(vaddr);
    uint64_t pdpt_phys = pml4[i4] & ~0xFFFULL;
    if (!pdpt_phys) return 0;
    uint64_t *pdpt = (uint64_t*)(uintptr_t)pdpt_phys;
    uint64_t pd_phys = pdpt[i3] & ~0xFFFULL;
    if (!pd_phys) return 0;
    uint64_t *pd = (uint64_t*)(uintptr_t)pd_phys;
    uint64_t pde = pd[i2];
    if (pde & 0x80ULL) return pde & (0xFFFULL | (1ULL << 63));
    uint64_t pt_phys = pde & ~0xFFFULL;
    if (!pt_phys) return 0;
    uint64_t *pt = (uint64_t*)(uintptr_t)pt_phys;
    uint64_t pte = pt[i1];
    if (!(pte & PTE_PRESENT)) return 0;
    return pte & (0xFFFULL | (1ULL << 63));
}

page_directory_t *paging_get_directory(void) {
    return &kernel_dir;
}

/* The directory the CPU is currently running under. Per-process map/restore
 * sequences must switch back HERE (not paging_get_directory(), which always
 * returns the kernel directory) so a process that traps into a syscall that
 * temporarily switches CR3 returns to user mode under its own directory. */
page_directory_t *paging_get_active(void) {
    return active_dir;
}

/* Debug: print the PML4 physical frame of the kernel directory and every
 * address space currently checked out of the slot pool. */
void paging_dump_dirs(void) {
    serial_printf("[PAGING] dirs: [kernel]=0x%lx",
                  (unsigned long)kernel_pml4_phys);
    for (page_directory_t *d = dir_live_list; d; d = d->next)
        serial_printf(" [0x%lx]", (unsigned long)d->pml4_phys);
    int free_count = 0;
    for (page_directory_t *f = dir_free_list; f; f = f->next) free_count++;
    serial_printf(" free=%d\n", free_count);
}

page_directory_t *paging_clone_directory(page_directory_t *src) {
    if (!src) return NULL;
    page_directory_t *dst = dir_free_list;
    if (!dst) return NULL;
    dir_free_list = dst->next;
    dst->next = NULL;
    dst->pml4_phys = 0;
    uint64_t src_pml4 = get_pml4_phys(src);
    uint64_t new_pml4 = pmm_alloc_page();
    if (!new_pml4) {
        dst->next = dir_free_list;
        dir_free_list = dst;
        return NULL;
    }
    memset((void*)(uintptr_t)new_pml4, 0, PAGE_SIZE);
    uint64_t *src_pml4_ptr = (uint64_t*)(uintptr_t)src_pml4;
    uint64_t *dst_pml4_ptr = (uint64_t*)(uintptr_t)new_pml4;
    for (int i = 0; i < 512; i++) {
        uint64_t e = src_pml4_ptr[i];
        if (!e || !(e & PTE_PRESENT)) {
            dst_pml4_ptr[i] = 0;
            continue;
        }
        uint64_t flags = e & 0xFFFULL;
        uint64_t src_pdpt = e & ~0xFFFULL;
        uint64_t dst_pdpt = pmm_alloc_page();
        if (!dst_pdpt) {
            dst_pml4_ptr[i] = 0;
            continue;
        }
        memset((void*)(uintptr_t)dst_pdpt, 0, PAGE_SIZE);
        uint64_t *src_pdpt_ptr = (uint64_t*)(uintptr_t)src_pdpt;
        uint64_t *dst_pdpt_ptr = (uint64_t*)(uintptr_t)dst_pdpt;
        for (int j = 0; j < 512; j++) {
            uint64_t e2 = src_pdpt_ptr[j];
            if (!e2 || !(e2 & PTE_PRESENT)) {
                dst_pdpt_ptr[j] = 0;
                continue;
            }
            uint64_t flags2 = e2 & 0xFFFULL;
            uint64_t src_pd = e2 & ~0xFFFULL;
            uint64_t dst_pd = pmm_alloc_page();
            if (!dst_pd) {
                dst_pdpt_ptr[j] = 0;
                continue;
            }
            memset((void*)(uintptr_t)dst_pd, 0, PAGE_SIZE);
            uint64_t *src_pd_ptr = (uint64_t*)(uintptr_t)src_pd;
            uint64_t *dst_pd_ptr = (uint64_t*)(uintptr_t)dst_pd;
            for (int k = 0; k < 512; k++) {
                uint64_t e3 = src_pd_ptr[k];
                if (!e3 || !(e3 & PTE_PRESENT)) {
                    dst_pd_ptr[k] = 0;
                    continue;
                }
                if (e3 & PTE_PS) {
                    dst_pd_ptr[k] = e3;
                    continue;
                }
                uint64_t flags3 = e3 & 0xFFFULL;
                uint64_t src_pt = e3 & ~0xFFFULL;
                uint64_t dst_pt = pmm_alloc_page();
                if (!dst_pt) {
                    dst_pd_ptr[k] = 0;
                    continue;
                }
                memset((void*)(uintptr_t)dst_pt, 0, PAGE_SIZE);
                uint64_t *src_pt_ptr = (uint64_t*)(uintptr_t)src_pt;
                uint64_t *dst_pt_ptr = (uint64_t*)(uintptr_t)dst_pt;
                for (int l = 0; l < 512; l++) {
                    uint64_t e4 = src_pt_ptr[l];
                    dst_pt_ptr[l] = e4;
                }
                dst_pd_ptr[k] = dst_pt | flags3;
            }
            dst_pdpt_ptr[j] = dst_pd | flags2;
        }
        dst_pml4_ptr[i] = dst_pdpt | flags;
    }
    dst->pml4_phys = new_pml4;
    dst->next = dir_live_list;
    dir_live_list = dst;
    return dst;
}

/* Release an address space. The caller must already have freed and unmapped
 * the process's data frames (process_destroy walks the user VMA for exactly
 * that reason): this reclaims only the page-table pages, which the clone owns
 * outright because cloning deep-copies the kernel's PML4/PDPT/PD subtrees
 * rather than sharing them. 2 MiB leaves are left alone -- they are identity
 * mappings, not allocations. The slot then returns to the pool. */
void paging_free_directory(page_directory_t *dir) {
    if (!dir || dir == &kernel_dir) return;
    uint64_t pml4_phys = dir->pml4_phys;
    if (!pml4_phys) return;

    uint64_t *pml4 = (uint64_t*)(uintptr_t)pml4_phys;
    for (int i = 0; i < 512; i++) {
        uint64_t e = pml4[i];
        if (!e || !(e & PTE_PRESENT)) continue;
        uint64_t *pdpt = (uint64_t*)(uintptr_t)(e & ~0xFFFULL);
        for (int j = 0; j < 512; j++) {
            uint64_t e2 = pdpt[j];
            if (!e2 || !(e2 & PTE_PRESENT)) continue;
            uint64_t *pd = (uint64_t*)(uintptr_t)(e2 & ~0xFFFULL);
            for (int k = 0; k < 512; k++) {
                uint64_t e3 = pd[k];
                if (!e3 || !(e3 & PTE_PRESENT) || (e3 & PTE_PS)) continue;
                pmm_free_page(e3 & ~0xFFFULL);
            }
            pmm_free_page(e2 & ~0xFFFULL);
        }
        pmm_free_page(e & ~0xFFFULL);
    }
    pmm_free_page(pml4_phys);

    dir->pml4_phys = 0;
    page_directory_t **pp = &dir_live_list;
    while (*pp) {
        if (*pp == dir) { *pp = dir->next; break; }
        pp = &(*pp)->next;
    }
    dir->next = dir_free_list;
    dir_free_list = dir;
}

void paging_switch_directory(page_directory_t *dir) {
    if (dir)
        active_dir = dir;
    write_cr3(get_pml4_phys(dir));
}