#include "sys/shm.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>

int main(){
    int shmid=shmget(1, 0x3f3f3f, IPC_CREAT | 0666);
    char *buffer=(char *)shmat(shmid, nullptr, 0);
    memset(buffer, '\0', 0x3f3f3f);
    int pid=fork();
    while(pid!=0){
        sleep(2);
        pid=fork();
    }
    sleep(100);
    return 0;
}