#include "libc.h"

/* Phase 2.4 guard-page test: recurses without a base case until the stack
 * pointer walks off the mapped region into the deliberately-unmapped guard
 * page. On a machine with SMEP the eager-copy pass must have left the page
 * directly below USER_STACK_BOTTOM unmapped; overflowing it must produce a
 * clean #14 (page fault) at a VA in [USER_STACK_BOTTOM, USER_STACK_BOTTOM+
 * PAGE_SIZE) rather than silently scribbling into adjacent mappings. */
static __attribute__((noinline)) void recurse(volatile int depth) {
    volatile char spill[4096];
    spill[0] = (char)depth;
    spill[1] = spill[4095];
    recurse(depth + 1);
    (void)spill[0];
}

int main(void) {
    printf_simple("stacktrip(%d): before guard-page overflow\n", getpid());
    recurse(0);
    return 0;
}
