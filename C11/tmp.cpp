#include <csignal>
#include <ctime>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "sort_time_lst.h"
#include <fcntl.h>
#include <errno.h>

int main(){
    int a=0x00006566;
    char *s=(char*)(&a);
    printf("%s %d\n",s,strlen(s));
    return 0;
}