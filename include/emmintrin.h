#ifndef _EMMINTRIN_H
#define _EMMINTRIN_H

typedef int __m128i __attribute__((__vector_size__(16)));

static inline __m128i _mm_loadu_si128(const void *p) {
    __m128i v;
    __builtin_memcpy(&v, p, 16);
    return v;
}

static inline void _mm_stream_si128(void *p, __m128i v) {
    __builtin_memcpy(p, &v, 16);
    __asm__ volatile("sfence" ::: "memory");
}

static inline void _mm_sfence(void) {
    __asm__ volatile("sfence" ::: "memory");
}

static inline __m128i _mm_set1_epi32(int i) {
    __m128i v = {i,i,i,i};
    return v;
}

#endif
