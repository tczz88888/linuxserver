#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENT_NUMBER 1000
#define BUFFER_SIZE 10

struct fds{
    int sockfd;
    int epollfd;
};
fds fd;

int setnoblocking(int fd){//设置非阻塞IO
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option|O_NONBLOCK;
    fcntl(fd, F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd,bool oneshot){
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET;
    if(oneshot) event.events|=EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnoblocking(fd);
}

void reset_oneshot(int epollfd,int fd){
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET|EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void* work(void *arg){
    int sockfd=((fds *)arg)->sockfd;
    int epollfd=((fds *)arg)->epollfd;
    printf("新线程开始读取文件描述符的数据%d",sockfd);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    while(1){
        int ret=recv(sockfd, buffer, BUFFER_SIZE-1, 0);
        if(ret==0){
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        }
        else if(ret<0){
            if(errno==EAGAIN){
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }
        else{
            printf("get content: %s\n",buffer);
            sleep(5);
        }
    }
    printf("线程结束读取文件描述符的数据%d",sockfd);
    return nullptr;
}


int main(int agrc,char *argc[]){

    return 0;
}