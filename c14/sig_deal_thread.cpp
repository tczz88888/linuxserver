#include "pthread.h"
#include "signal.h"
#include "unistd.h"
#include "errno.h"
#include <bits/types/sigset_t.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#define handle_error_en(en,msg) \
    do {errno=en;perror(msg);exit(EXIT_FAILURE);}while(0)
void * sig_thread(void *arg){
    sigset_t* set=(sigset_t *) arg;
    int sig,s;
    while(1){
        s=sigwait(set, &sig);
        if(s!=0){
            errno=s;
            handle_error_en(s, "sig_wait");
        }
        printf("signal handing thread go signal %d\n",sig);
    }
}


int main(){
    pthread_t thread;
    sigset_t set;
    int s;
    /*主线程设置信号掩码*/
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGINT);

    s=pthread_sigmask(SIG_BLOCK, &set, nullptr);
    if(s!=0){
        handle_error_en(s, "pthread_sigmask");
    }
    s=pthread_create(&thread, nullptr, sig_thread, (void *)&set);
    if(s!=0){
        handle_error_en(s, "pthread_create");
    }
    pause();
    return 0;
}