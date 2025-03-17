#include <cerrno>
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
#include "timer_heap.h"
#include <fcntl.h>
#include <errno.h>

const int MAX_EVENTS=15;
const int MAX_FDS=20;
const int TIMESLOT=5;
const int DEFAULT_HEAP_SIZE=20;
client_data *users;
epoll_event *events;
static timer_heap heap(DEFAULT_HEAP_SIZE);
static int pipefd[2];//用来获取信号的信息
static int epollfd=0;
static int user_cnt=0;


int setNoBlocking(int fd){
    int oldoption=fcntl(fd,F_GETFL);
    int newoption=oldoption|O_NONBLOCK;
    fcntl(fd, F_SETFL,newoption);
    return oldoption;
}

void addfd(int epollfd,int fd){
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLERR;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNoBlocking(fd);
}

void sig_handle(int sig){
    int save_errno=errno;
    int msg=sig;
    send(pipefd[1],reinterpret_cast<char *>(&msg),1,0);
    errno=save_errno;
}

void addsig(int sig){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler=sig_handle;
    sa.sa_flags|=SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr)!=-1);
}

void timer_handler(){
    heap.tick();
    alarm(TIMESLOT);
}


void cb_func(client_data *user_data){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n",user_data->sockfd);
}

int main(int argc,char *argv[]){
    if(argc<=2){
        printf("usage :%s ip port",basename(argv[0]));
        return -1;
    }
    char *ip=argv[1];
    int port=atoi(argv[2]);
    sockaddr_in  listen_addr;
    bzero(&listen_addr, sizeof(listen_addr));
    listen_addr.sin_port=htons(port);
    listen_addr.sin_family=AF_INET;
    inet_pton(AF_INET, ip, &listen_addr.sin_addr);
    int listenfd=socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd>=0);

    int ret=bind(listenfd, (sockaddr *)&listen_addr, sizeof(listen_addr));
    assert(ret!=-1);

    ret=listen(listenfd, 5);


    events=new epoll_event[MAX_EVENTS];
    users=new client_data[MAX_FDS];
    epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd, listenfd);

    ret=socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret!=-1);
    setNoBlocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server=false; 
    bool timeout=false;
    alarm(TIMESLOT);

    while(!stop_server){
        int numbers=epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if(ret<0&&errno!=EINTR){
            puts("epoll failure");
            break;
        }
        for(int i=0;i<numbers;i++){
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd){
                sockaddr_in client_addr;
                socklen_t cilent_len=sizeof(client_addr);
                int connfd=accept(listenfd, (sockaddr *)&client_addr, &cilent_len);
                if(connfd==-1){
                    printf("%d %d\n",listenfd,errno);
                    return -1;
                }
                printf("client connect %d\n",connfd);
                addfd(epollfd, connfd);
                users[connfd].client_addr=client_addr;
                users[connfd].sockfd=connfd;

                heap_timer *client_timer=new heap_timer(3*TIMESLOT);
                client_timer->user_data=&users[connfd];
                client_timer->cb_func=cb_func;
                users[connfd].timer=client_timer;
                heap.add_timer(client_timer);
            }
            else if(sockfd==pipefd[0]&&(events[i].events&EPOLLIN)){
                int sig;
                char signals[1024];
                ret=recv(sockfd, signals, 1023, 0);
                if(ret<0){
                    //可以捕获一下
                    continue;
                }
                else if(ret==0){
                    continue;
                }
                else{
                    for(int i=0;i<ret;i++){
                        switch (signals[i]) {
                            case SIGALRM:
                            {
                                timeout=true;//这个为true的时候就要处理一下定时任务
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server=true;
                            }
                        }
                    }
                }
                
            }
            else if(events[i].events&EPOLLIN){
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret=recv(sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0);
                heap_timer *timer=users[sockfd].timer;
                if(ret<0){
                    if(errno!=EAGAIN){
                        cb_func(&users[sockfd]);
                        if(timer){
                            heap.delete_timer(timer);
                        }
                    }
                }
                else if(ret==0){
                    cb_func(&users[sockfd]);
                    if(timer){
                            heap.delete_timer(timer);
                    }
                }
                else{
                    if(timer){
                        int delay=3*TIMESLOT;
                        printf("get info %s frmo fd %d ,adjust timer once\n",users[sockfd].buf,sockfd);
                        heap.adjust_timer(timer,delay);
                    }
                }
            }
            else{
                //TODO
            }
        }
        if(timeout){
            timer_handler();
            timeout=false;
        }
    }
    return 0;
}