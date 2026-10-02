#include "libc.h"

void spawn_daemon(const char *path) {
    printf("[myinit] Starting %s...\n", path);
    int ret = execve(path, 0, 0);
    if (ret < 0) {
        printf("[myinit] Failed to start %s\n", path);
    } else {
        printf("[myinit] Started %s with PID %d\n", path, ret);
    }
}

int main(void) {
    puts("[myinit] PID 1 system daemon initialized.");

    // Spawn core system daemons
    spawn_daemon("/sbin/logd");
    spawn_daemon("/sbin/devmgr");
    spawn_daemon("/sbin/mountd");
    spawn_daemon("/sbin/netmgr");
    spawn_daemon("/sbin/audiosrv");
    spawn_daemon("/sbin/powersrv");

    // Wait for children to exit (reap zombies)
    while (1) {
        int status;
        pid_t pid = wait(-1, &status);
        if (pid > 0) {
            printf("[myinit] Process %d exited with status %d\n", pid, status);
        } else {
            yield();
        }
    }
    return 0;
}
