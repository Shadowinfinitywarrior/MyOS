#ifndef TYPES_H
#define TYPES_H

typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned short      uint16_t;
typedef signed short        int16_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
typedef unsigned long long  uint64_t;
typedef signed long long    int64_t;

typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef int64_t             ptrdiff_t;
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;
typedef int32_t             pid_t;
typedef int32_t             off_t;
typedef uint32_t            mode_t;
typedef uint32_t            ino_t;
typedef int32_t             dev_t;

typedef enum { false = 0, true = 1 } bool;

#define NULL ((void *)0)

#define PACKED          __attribute__((packed))
#define ALIGNED(x)      __attribute__((aligned(x)))
#define UNUSED          __attribute__((unused))
#define NORETURN        __attribute__((noreturn))
#define ALWAYS_INLINE   __attribute__((always_inline)) inline
#define SECTION(x)      __attribute__((section(x)))

#define BIT(x)              (1U << (x))
#define SET_BIT(val, bit)   ((val) | BIT(bit))
#define CLEAR_BIT(val, bit) ((val) & ~BIT(bit))
#define TEST_BIT(val, bit)  ((val) & BIT(bit))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ALIGN_UP(val, align)   (((val) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(val, align) ((val) & ~((align) - 1))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define PAGE_SIZE 4096

#define barrier() __asm__ __volatile__("" ::: "memory")

#endif
