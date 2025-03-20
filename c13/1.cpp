#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <arpa/inet.h>
#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65536

struct client_data{//user对应的addr，fd，进程号，管道
    sockaddr_in address;
    int connfd;
    pid_t pid;
    int pipefd[2];
};

static const char* shm_name="/my_shm";//共享内存的名字
int sig_pipefd[2];//信号往这里走
int epollfd;
int listenfd;
int shmfd;//共享内存对应的文件描述符
char *share_mem=nullptr;//共享内存的起始地址
client_data* users=nullptr;//以用户连接的编号为索引
int *sub_process=nullptr;//以进程号为索引，得到进程号对应的用户连接编号
int user_count=0;
bool stop_child=false;

int setnoblocking(int fd){
    int oldoption=fcntl(fd,F_GETFL);
    int newoption=oldoption|O_NONBLOCK;
    fcntl(fd, F_SETFL,newoption);
    return oldoption;
}

void addfd(int epollfd,int fd){
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnoblocking(fd);
}

void sig_handle(int sig){
    int save_errno=errno;
    int msg=sig;
    send(sig_pipefd[1], (char *)&msg, 1, 0);
    errno=save_errno;
}

void addsig(int sig,void(*handle)(int),bool restart=true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler=handle;
    if(restart){
        sa.sa_flags|=SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr)!=-1);
}

void del_resource(){
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(epollfd);
    close(listenfd);
    shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}

void child_term_handler(int sig){
    stop_child=true;
}

int run_child(int idx,client_data *users,char *share_mem){
    epoll_event event[MAX_EVENT_NUMBER];
    int child_epollfd=epoll_create(5);
    assert(child_epollfd!=-1);
    int connfd=users[idx].connfd;
    int pipefd=users[idx].pipefd[1];
    addfd(child_epollfd, connfd);//接收客户端发的信息
    addfd(child_epollfd, pipefd);//通知父进程客户端来信息了，然后父进程分发给其他客户端对应的子线程
    int ret;
    addsig(SIGTERM, child_term_handler,false);

    while(!stop_child){
        ret=epoll_wait(child_epollfd, event, MAX_EVENT_NUMBER, -1);
        if(ret<0&&errno!=EINTR){
            printf("child epoll failure\n");
            break;
        }

        for(int i=0;i<ret;i++){
            int sockfd=event[i].data.fd;
            if(sockfd==connfd&&(event[i].events&EPOLLIN)){
                memset(share_mem+idx*BUFFER_SIZE, '\0', BUFFER_SIZE);
                ret=recv(connfd, share_mem+idx*BUFFER_SIZE, BUFFER_SIZE-1, 0);
                if(ret<0){
                    if(ret!=EAGAIN){
                        stop_child=true;
                    }
                }
                else if(ret==0){
                    stop_child=true;
                }
                else{
                    send(pipefd, &idx, sizeof(idx), 0);
                }
            }
            else if(sockfd==pipefd&&(event[i].events&EPOLLIN)){
                int client=0;
                ret=recv(pipefd,&client,sizeof(client),0);
                if(ret<0){
                    if(ret!=EAGAIN){
                        stop_child=true;
                    }
                }
                else if(ret==0){
                    stop_child=true;
                }
                else{
                    send(connfd, share_mem+client*BUFFER_SIZE , BUFFER_SIZE , 0);
                }
            }
            else{
                continue;
            }
        }
    }
    close(child_epollfd);
    close(pipefd);
    close(connfd);
    return 0;
}


int main(int argc,char *argv[]){
    if(argc<=2){
        printf("usage=%s ip_address port\n",basename(argv[0]));
        return -1;
    }
    char *ip_address=argv[1];
    int port=atoi(argv[2]);

    sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);
    inet_pton(AF_INET, ip_address, &server_addr.sin_addr);
    
    listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);
    int ret=bind(listenfd, (sockaddr *)&server_addr, sizeof(server_addr));
    assert(ret!=-1);

    ret=listen(listenfd,5);
    assert(ret!=-1);

    user_count=0;
    users=new client_data[USER_LIMIT];
    sub_process=new int[PROCESS_LIMIT];
    for(int i=0;i<PROCESS_LIMIT;i++) {
        sub_process[i]=-1;
    }
    epoll_event event[MAX_EVENT_NUMBER];
    epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd, listenfd);

    ret=socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret!=-1);
    setnoblocking(sig_pipefd[1]);
    addfd(epollfd, sig_pipefd[0]);
    addsig(SIGCHLD, sig_handle);
    addsig(SIGTERM, sig_handle);
    addsig(SIGINT, sig_handle);
    addsig(SIGPIPE, SIG_IGN);
    bool stop_server=false;
    bool terminate=false;   

    shmfd=shm_open(shm_name, O_CREAT|O_RDWR, 0666);
    assert(shmfd!=-1);

    ret=ftruncate(shmfd, USER_LIMIT*BUFFER_SIZE);
    assert(ret!=-1);

    share_mem=(char *)mmap(nullptr, USER_LIMIT*BUFFER_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem!=MAP_FAILED);
    close(shmfd);

    while(!stop_server){
        ret=epoll_wait(epollfd, event, MAX_EVENT_NUMBER, -1);
        if(ret<0&&!(errno!=EINTR)){
            printf("epoll failure\n");
            break;
        }
        
        for(int i=0;i<ret;i++){
            int sockfd=event[i].data.fd;

            if(sockfd==listenfd){

            }
            else if(sockfd==sig_pipefd[0]&&(event[i].events&EPOLLIN)){

            }
            else if(event[i].events&EPOLLIN){
                
            }
        }
    }

    del_resource();
    return 0;
}