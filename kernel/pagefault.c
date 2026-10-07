#include "pagefault.h"
#include "paging.h"
#include "pmm.h"
#include "process.h"
#include "heap.h"
#include "shm.h"
#include "../lib/printf.h"
#include "../lib/string.h"

#define PAGE_SIZE 4096

/* Page fault error code bits */
#define PF_PRESENT    (1 << 0)  /* Page present (protection violation) */
#define PF_WRITE      (1 << 1)  /* Write access */
#define PF_USER       (1 << 2)  /* User-mode access */
#define PF_RSVD       (1 << 3)  /* Reserved bit set */
#define PF_INSTR      (1 << 4)  /* Instruction fetch */

/* Memory protection flags (from mmap.h) */
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
#define PROT_NONE   0x0

/* mmap flags */
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

/* User address space bounds */
#define USER_ADDR_MIN 0x40000000ULL
/* USER_STACK_TOP, USER_STACK_SIZE, USER_STACK_BOTTOM from process.h */
#define GUARD_PAGE_ADDR (USER_STACK_BOTTOM - PAGE_SIZE)

/* VMA tracking for demand paging and COW */
typedef struct vma_entry {
    uint64_t start;
    uint64_t end;
    uint32_t prot;
    uint32_t flags;
    int fd;
    uint64_t offset;
    bool cow;
    struct vma_entry *next;
} vma_entry_t;

static vma_entry_t *vma_list = NULL;

void pagefault_init(void) {
    vma_list = NULL;
    kprintf("[PAGEFAULT] Page fault handler initialized with COW support\n");
}

/* Add a VMA entry for tracking mappings */
void vma_add(uint64_t start, uint64_t end, uint32_t prot, uint32_t flags, int fd, uint64_t offset) {
    vma_entry_t *vma = (vma_entry_t *)kmalloc(sizeof(vma_entry_t));
    vma->start = start;
    vma->end = end;
    vma->prot = prot;
    vma->flags = flags;
    vma->fd = fd;
    vma->offset = offset;
    vma->cow = (flags & MAP_PRIVATE) && (prot & PROT_WRITE);
    vma->next = NULL;

    vma_entry_t **pp = &vma_list;
    while (*pp && (*pp)->start < start) pp = &(*pp)->next;
    vma->next = *pp;
    *pp = vma;
}

/* Remove a VMA entry */
void vma_remove(uint64_t start, uint64_t end) {
    vma_entry_t **pp = &vma_list;
    while (*pp) {
        if ((*pp)->start >= start && (*pp)->end <= end) {
            vma_entry_t *tmp = *pp;
            *pp = tmp->next;
            kfree(tmp);
        } else {
            pp = &(*pp)->next;
        }
    }
}

/* Update VMA protection (for mprotect) */
void vma_update_prot(uint64_t start, uint64_t end, uint32_t prot) {
    vma_entry_t *vma = vma_list;
    while (vma) {
        if (vma->start == start && vma->end == end) {
            vma->prot = prot;
            vma->cow = (vma->flags & MAP_PRIVATE) && (prot & PROT_WRITE);
            break;
        }
        vma = vma->next;
    }
}

/* Find VMA containing address */
static vma_entry_t *vma_find(uint64_t addr) {
    vma_entry_t *v = vma_list;
    while (v) {
        if (addr >= v->start && addr < v->end) return v;
        v = v->next;
    }
    return NULL;
}

/* Handle copy-on-write fault */
static int handle_cow_fault(vma_entry_t *vma, uint64_t fault_addr, uint64_t error_code) {
    if (!(error_code & PF_WRITE)) return -1;  /* Not a write fault */
    if (!vma->cow) return -1;                 /* Not a COW mapping */

    uint64_t page_addr = fault_addr & ~(PAGE_SIZE - 1);
    uint64_t phys = paging_get_physical(page_addr);
    if (!phys) return -1;

    /* Allocate new page */
    uint64_t new_phys = pmm_alloc_page();
    if (!new_phys) return -1;

    /* Copy content */
    memcpy((void *)(uintptr_t)new_phys, (void *)(uintptr_t)phys, PAGE_SIZE);

    /* Remap with write permission */
    uint64_t page_flags = PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
    if (!(vma->prot & PROT_EXEC)) page_flags |= PAGE_NX;
    paging_map(page_addr, new_phys, page_flags);

    /* Free old page if no other mappings reference it (simplified) */
    pmm_free_page(phys);

    kprintf("[PAGEFAULT] COW: addr=0x%llx new_phys=0x%llx\n", (unsigned long long)fault_addr, (unsigned long long)new_phys);
    return 0;
}

/* Handle demand paging (lazy allocation) */
static int handle_demand_page(vma_entry_t *vma, uint64_t fault_addr, uint64_t error_code) {
    (void)error_code;
    uint64_t page_addr = fault_addr & ~(PAGE_SIZE - 1);
    uint64_t phys = pmm_alloc_page();
    if (!phys) return -1;

    memset((void *)(uintptr_t)phys, 0, PAGE_SIZE);

    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    if (vma->prot & PROT_WRITE) page_flags |= PAGE_WRITE;
    if (!(vma->prot & PROT_EXEC)) page_flags |= PAGE_NX;

    paging_map(page_addr, phys, page_flags);
    return 0;
}

/* Handle stack growth (guard page) */
static int handle_stack_growth(uint64_t fault_addr, uint64_t error_code) {
    if (fault_addr < USER_STACK_BOTTOM) return -1;
    if (fault_addr >= USER_STACK_TOP) return -1;
    if (!(error_code & PF_WRITE)) return -1;  /* Stack growth only on write */

    /* Allocate the faulting page */
    uint64_t page_addr = fault_addr & ~(PAGE_SIZE - 1);
    uint64_t phys = pmm_alloc_page();
    if (!phys) return -1;

    memset((void *)(uintptr_t)phys, 0, PAGE_SIZE);
    paging_map(page_addr, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER | PAGE_NX);

    kprintf("[PAGEFAULT] Stack growth: addr=0x%llx\n", (unsigned long long)fault_addr);
    return 0;
}

/* Page fault handler - called from ISR */
void pagefault_handler(registers_t *regs) {
    uint64_t fault_addr;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_addr));
    uint64_t error_code = regs->err_code;

    /* Ignore kernel-mode faults for now (panic) */
    if (!(error_code & PF_USER)) {
        kprintf("[PAGEFAULT] KERNEL PAGE FAULT!\n");
        kprintf("[PAGEFAULT]   fault_addr=0x%llx\n", (unsigned long long)fault_addr);
        kprintf("[PAGEFAULT]   error_code=0x%llx\n", (unsigned long long)error_code);
        kprintf("[PAGEFAULT]   rip=0x%llx\n", (unsigned long long)regs->rip);
        kprintf("[PAGEFAULT]   cs=0x%llx\n", (unsigned long long)regs->cs);
        kprintf("[PAGEFAULT]   rflags=0x%llx\n", (unsigned long long)regs->rflags);
        kprintf("[PAGEFAULT]   rsp=0x%llx\n", (unsigned long long)regs->rsp);
        kprintf("[PAGEFAULT]   PF_PRESENT=%d PF_WRITE=%d PF_USER=%d PF_RSVD=%d PF_INSTR=%d\n",
                (error_code & PF_PRESENT) != 0,
                (error_code & PF_WRITE) != 0,
                (error_code & PF_USER) != 0,
                (error_code & PF_RSVD) != 0,
                (error_code & PF_INSTR) != 0);
        
        process_t *proc = process_get_current();
        if (proc) {
            kprintf("[PAGEFAULT]   current process: '%s' (pid=%d)\n", proc->name, proc->pid);
        }
        
        /* Try to disassemble the faulting instruction */
        kprintf("[PAGEFAULT] Faulting instruction bytes: ");
        uint8_t *insn = (uint8_t*)regs->rip;
        for (int i = 0; i < 16; i++) {
            kprintf("%02x ", insn[i]);
        }
        kprintf("\n");
        
        for (;;) hlt();
    }

    /* Check if it's in user address space */
    if (fault_addr < USER_ADDR_MIN) {
        kprintf("[PAGEFAULT] Fault below user space: 0x%llx\n", (unsigned long long)fault_addr);
        process_kill(process_get_current()->pid, 11);  /* SIGSEGV */
        return;
    }

    /* Handle stack guard page / growth */
    if (fault_addr >= USER_STACK_BOTTOM && fault_addr < USER_STACK_TOP) {
        if (handle_stack_growth(fault_addr, error_code) == 0) return;
    }

    /* Check VMA list for this address */
    vma_entry_t *vma = vma_find(fault_addr);
    if (vma) {
        /* Try COW first */
        if (handle_cow_fault(vma, fault_addr, error_code) == 0) return;

        /* Try demand paging */
        if (handle_demand_page(vma, fault_addr, error_code) == 0) return;
    }

    /* Check for shared memory mappings */
    extern shm_segment_t *shm_get_segments(void);
    shm_segment_t *segments = shm_get_segments();
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (segments[i].in_use) {
            uint32_t shm_virt_base = 0xC0000000 + i * 0x100000;
            uint32_t shm_virt_end = shm_virt_base + segments[i].size;
            if (fault_addr >= shm_virt_base && fault_addr < shm_virt_end) {
                /* Map the shared memory page */
                uint64_t page_offset = (fault_addr - shm_virt_base) & ~(PAGE_SIZE - 1);
                uint64_t phys = segments[i].phys_addr + page_offset;
                paging_map(fault_addr & ~(PAGE_SIZE - 1), phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                return;
            }
        }
    }

    /* Unhandled fault - send SIGSEGV */
    kprintf("[PAGEFAULT] Unhandled fault at 0x%llx (error=0x%llx), killing process\n",
            (unsigned long long)fault_addr, (unsigned long long)error_code);
    process_kill(process_get_current()->pid, 11);  /* SIGSEGV */
}