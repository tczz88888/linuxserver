#include "threadpool.h"
#include "http_conn.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
extern int addfd(int epollfd,int fd,bool one_shot);
extern int removefd(int epollfd,int fd);

void addsig(int sig,void (handle)(int),bool restart=true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler=handle;
    if(restart){
        sa.sa_flags|=SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr)!=-1);
}

void show_error(int connfd,const char* info){
    printf("%s\n",info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc,char* argv[]){
    if(argc<=2){
        printf("usage:%s ip prot\n",basename(argv[0]));
        return 1;
    }
    char *ip=argv[1];
    int port=atoi(argv[2]);
    
    /*忽略SIGPIPE信号*/
    addsig(SIGPIPE, SIG_IGN);
    
    /*创建线程池*/
    threadpool<http_conn>*pool=NULL;
    try{
        pool=new threadpool<http_conn>;
    }
    catch(...){
        return 1;
    }
    http_conn* users=new http_conn[MAX_FD];
    assert(users);
    http_conn::m_user_count=0;
    // http_conn::have_done=0;

    int listenfd=socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd>=0);

    linger tmp={1,0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret=0;
    sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_port=htons(port);
    server_addr.sin_family=AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    ret=bind(listenfd, (sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret>=0);

    ret=listen(listenfd, 5);
    assert(ret>=0);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd=epoll_create(5);
    addfd(epollfd, listenfd, false);

    http_conn::m_epollfd=epollfd;

    while(true){
        int number=epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number<0&&(errno!=EINTR)){
            printf("epoll failure\n");
            break;
        }

        for(int i=0;i<number;i++){
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd){
                sockaddr_in client_addr;
                socklen_t len;
                int connfd=accept(listenfd, (sockaddr *)&client_addr, &len);
                if(connfd<0){
                    printf("error is: %d\n",errno);
                    continue;
                }
                else if(http_conn::m_user_count>=MAX_FD){
                    show_error(connfd, "Internal server busy");
                    continue;;
                }
                printf("a client in.....\n");
                users[connfd].init(connfd, client_addr);
            }
            else if(events[i].events&(EPOLLRDHUP|EPOLLERR|EPOLLHUP)){
                users[sockfd].close_conn();
                printf("a client gone.....\n");
            }
            else if(events[i].events&EPOLLIN){
                if(users[sockfd].read()){
                    printf("read successful from %d\n",sockfd);
                    pool->append(users+sockfd);
                }
                else{
                    users[sockfd].close_conn();
                }
            }
            else if(events[i].events&EPOLLOUT){
                if(!users[sockfd].write()){
                    users[sockfd].close_conn();
                }
                printf("have done %d\n",http_conn::m_done);
            }
            else{

            }
        }
    }
    close(listenfd);
    close(epollfd);
    delete [] users;
    delete pool;
    return 0;
}