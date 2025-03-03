#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENT_NUMBER 1000
#define BUFFER_SIZE 10

struct fds {
  int sockfd;
  int epollfd;
};
fds fd;

int setnoblocking(int fd) { // 设置非阻塞IO
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

void addfd(int epollfd, int fd, bool oneshot) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET;
  if (oneshot)
    event.events |= EPOLLONESHOT;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnoblocking(fd);
}

void reset_oneshot(int epollfd, int fd) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void *work(void *arg) {
  int sockfd = ((fds *)arg)->sockfd;
  int epollfd = ((fds *)arg)->epollfd;
  pthread_t thread=pthread_self();
  printf("新线程%ld开始读取%d文件描述符的数据", thread,sockfd);
  char buffer[BUFFER_SIZE];
  while (1) {
  memset(buffer, 0, sizeof(buffer));
    int ret = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (ret == 0) {
      close(sockfd);
      printf("foreiner closed the connection\n");
      break;
    } else if (ret < 0) {
      if (errno == EAGAIN) {
        reset_oneshot(epollfd, sockfd);
        printf("read later\n");
        break;
      }
    } else {
      printf("get content: %s,len :%d\n", buffer,ret);
      sleep(2);
    }
  }
  printf("线程结束读取文件描述符的数据%d", sockfd);
  return nullptr;
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    printf("usage:%s ip port", basename(argv[0]));
    return -1;
  }
  char *ip = argv[1];
  int port = atoi(argv[2]);
  int listenfd = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &server_addr.sin_addr);
  int ret =
      bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  assert(ret != -1);
  ret = listen(listenfd, 5);
  assert(ret != -1);
  epoll_event events[MAX_EVENT_NUMBER];
  int epollfd = epoll_create(5);
  assert(epollfd != -1);
  addfd(epollfd, listenfd, false); // 监听listenfd这个文件描述符
  while (1) {
    int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
    if(num<0){
      printf("epoll error\n");
      break;
    }
    for(int i=0;i<num;i++){
      int sockfd=events[i].data.fd;
      if(sockfd==listenfd){//新的连接进来了
        sockaddr_in client_sockaddr;
        socklen_t sockaddr_len;
        int connfd=accept(listenfd, (struct sockaddr *)&client_sockaddr, &sockaddr_len);
        addfd(epollfd, connfd, true);//读数据的socket都要打上epolloneshot
        printf("new connect %d\n",connfd);
      }
      else if(events[i].events&EPOLLIN){
        pthread_t thread;
        fds fds_for_works;
        fds_for_works.epollfd=epollfd;
        fds_for_works.sockfd=events[i].data.fd;
        pthread_create(&thread, NULL, work, (void *)&fds_for_works);
      }
      else{
        printf("something else happnend\n");
      }
    }
  }
  close(listenfd);
  return 0;
}