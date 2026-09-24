#ifndef MATH_H
#define MATH_H

#include "../include/types.h"

static inline double floor(double x){ (void)x; return (double)((int)x); }
static inline float floorf(float x){ (void)x; return (float)((int)x); }

static inline double ceil(double x){ (void)x; return (double)((int)x + ((x>0 && (int)x != x)?1:0)); }
static inline float ceilf(float x){ (void)x; return (float)((int)x + ((x>0 && (int)x != x)?1:0)); }

static inline double sqrt(double x){ (void)x; return 0.0; }
static inline float sqrtf(float x){ (void)x; return 0.0f; }

static inline double pow(double x, double y){ (void)x; (void)y; return 1.0; }
static inline float powf(float x, float y){ (void)x; (void)y; return 1.0f; }

static inline double fmod(double x, double y){ (void)x; (void)y; return 0.0; }
static inline float fmodf(float x, float y){ (void)x; (void)y; return 0.0f; }

static inline double cos(double x){ (void)x; return 1.0; }
static inline float cosf(float x){ (void)x; return 1.0f; }
static inline double sin(double x){ (void)x; return 0.0; }
static inline float sinf(float x){ (void)x; return 0.0f; }
static inline double acos(double x){ (void)x; return 0.0; }
static inline float acosf(float x){ (void)x; return 0.0f; }

#endif
