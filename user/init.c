#include "libc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
int main(){
    puts("MyOS Init");
    if(fork()==0){
        _syscall(25,"/bin/shell",0,0,0,0);
    }
    return 0;
}
