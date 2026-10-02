#ifndef MYOSINFO_H
#define MYOSINFO_H

/*
 * Shared information structures.
 *
 * The kernel fills these in and the shell reads them, so the layout has to be
 * identical on both sides. Keeping them in one header (instead of duplicating
 * them in kernel/syscall.c and user/libc.h) is what makes that safe.
 *
 * Everything here is plain data: fixed-size arrays, no pointers. That means a
 * user program can hand the kernel a buffer and get it filled without the
 * kernel needing to know how user memory is laid out.
 */

/* One PCI device found by the bus scan. */
typedef struct {
    unsigned short vendor;    /* Vendor ID   (e.g. 0x8086 = Intel)   */
    unsigned short device;    /* Device ID                            */
    unsigned char  bus;       /* PCI bus number                       */
    unsigned char  slot;      /* PCI slot (device) number             */
    unsigned char  func;      /* Function number within the slot      */
    unsigned char  irq;       /* Legacy interrupt line                */
    unsigned char  class;     /* Device class code                    */
    unsigned char  subclass;  /* Device subclass code                 */
    unsigned char  revision;  /* Revision ID                          */
    unsigned int   bar0;      /* First Base Address Register (masked) */
} myos_pci_info_t;

/* Memory and processor summary. */
typedef struct {
    unsigned long total_pages;      /* Every physical page the firmware reported */
    unsigned long free_pages;       /* Pages the allocator can still hand out    */
    unsigned long total_ram_bytes;  /* total_pages * page size                   */
    unsigned long free_ram_bytes;   /* free_pages  * page size                   */
    unsigned int  page_size;        /* Bytes per page (normally 4096)            */
    unsigned int  cpu_count;        /* Logical processors detected               */
    unsigned long uptime_secs;      /* Seconds since boot                        */
    unsigned long heap_used_bytes;  /* Bytes currently taken from the kernel heap */
} myos_sysinfo_t;

/* Processor identity, straight from the CPUID instruction. */
typedef struct {
    char          vendor[16];   /* "GenuineIntel", "AuthenticAMD", ... */
    char          brand[52];    /* Marketing name string              */
    unsigned int  family;       /* CPU family                         */
    unsigned int  model;        /* CPU model                          */
    unsigned int  stepping;     /* CPU stepping                       */
    unsigned int  features_ecx; /* CPUID leaf 1, ECX feature bits     */
    unsigned int  features_edx; /* CPUID leaf 1, EDX feature bits     */
    unsigned int  logical_cores;/* Logical processors, from CPUID 1   */
} myos_cpuinfo_t;

/* Wall clock time read from the real-time clock chip. */
typedef struct {
    unsigned short year;    /* Full year, e.g. 2026 */
    unsigned char  month;   /* 1-12                 */
    unsigned char  day;     /* 1-31                 */
    unsigned char  hour;    /* 0-23                 */
    unsigned char  minute;  /* 0-59                 */
    unsigned char  second;  /* 0-59                 */
    unsigned char  weekday; /* 0-6, 0 = Sunday      */
} myos_datetime_t;

/* A short description of a supported driver. */
typedef struct {
    char name[24];      /* Short name, e.g. "keyboard"   */
    char category[16];  /* "input", "storage", "net", ... */
    char status[24];    /* "ready", "absent", "stub"      */
    char description[48]; /* One plain-English sentence   */
} myos_driver_info_t;

/* Screen / framebuffer description. */
typedef struct {
    unsigned int  width;    /* Pixels across   */
    unsigned int  height;   /* Pixels down     */
    unsigned int  bpp;      /* Bits per pixel  */
    unsigned int  pitch;    /* Bytes per row   */
    unsigned long fb_size;  /* Total framebuffer bytes */
    int           enabled;  /* 1 if a framebuffer console is active */
    int           text_mode;/* 1 if we fell back to VGA text mode   */
} myos_fbinfo_t;

/* File information, filled in by the stat system call. */
typedef struct {
    int           is_dir;   /* 1 if this is a directory, 0 for a file */
    unsigned int  size;     /* Size in bytes (0 for directories)      */
} myos_stat_t;

#endif
