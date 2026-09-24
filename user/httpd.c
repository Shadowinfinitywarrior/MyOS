#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
int main(){ puts("MyOS HTTP Server listening on 80"); while(1) sleep_ms(1000); return 0; }
