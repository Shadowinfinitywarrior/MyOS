#include "libc.h"
int main(){
    puts("MyOS Init");
    if(fork()==0){
        _syscall(25, (long)"/bin/shell", 0, 0, 0, 0);
    }
    return 0;
}
