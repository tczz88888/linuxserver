/*
#include <sys/select.h>

int select(int nfds, fd_set *_Nullable restrict readfds,
                  fd_set *_Nullable restrict writefds,
                  fd_set *_Nullable restrict exceptfds,
                  struct timeval *_Nullable restrict timeout);
参数：nfds：监听的文件描述符总数，一般被设置为select监听的所有文件描述符中的最大值+1
     readfds,writefds和exceptfds分别指向可读，可写和异常事件对应的文件描述符集合，向这三个参数传入自己要用的文件描述符
     select返回时会修改他们，来通知那些文件描述符已经准备就绪
     timeout就是超时时间，秒数，微妙数

返回值：成功时返回就绪的文件描述符总数，超时时间内没有任何文件描述符就绪时返回0，失败返回-1并且设置errno，在select等待期间，程序
       收到信号，则select立即返回-1，并且设置errno为EINTR

fd_set是一个整形数组，每一个元素的每一位标记一个文件描述符，所以需要用位运算，以下函数方便我们操作

void FD_CLR(int fd, fd_set *set);清除fd_set的fd位
int  FD_ISSET(int fd, fd_set *set);测试fd_set的fd位是否被设置
void FD_SET(int fd, fd_set *set);设置fd_set的fd位
void FD_ZERO(fd_set *set);清除fd_set的所有位


#include <poll.h>

       int poll(struct pollfd *fds, nfds_t nfds, int timeout);
       参数:
fds是一个链表，每个节点保存着一个文件描述符的信息，nfds同上，timeout是超时时间，单位ms
       struct pollfd {
               int   fd;        文件描述符
               short events;     监听的事件
               short revents;    实际发生的事件
           };


*/

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
const int BUFFER_SIZE = 10000000;
char buffer[BUFFER_SIZE];
int main(int argc, char *argv[]) {
  if (argc <= 2) {
    printf("usage %s ip_address port", basename(argv[0]));
  }
  char *ip = argv[1];
  int port = atoi(argv[2]);
  int sockfd = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &server_addr.sin_addr);
  int ret = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  assert(ret != -1);
  ret = listen(sockfd, 5);
  assert(ret != -1);
  sockaddr_in client_addr;
  socklen_t len;
  int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
  if (connfd == -1) {
    printf("connect failed\n");
    exit(1);
  } else {
    while (1) {
      fd_set read_fds;
      fd_set write_fds;
      fd_set except_fds;
      timeval tle;
      FD_ZERO(&read_fds);
      FD_ZERO(&write_fds);
      FD_ZERO(&except_fds);
      FD_SET(connfd, &read_fds);
      tle.tv_sec = 100;
      tle.tv_usec = 0;
      int ok = select(connfd + 1, &read_fds, &write_fds, &except_fds, &tle);
      if (FD_ISSET(connfd, &read_fds)) {
        int sum = 0;
        memset(buffer, '\0', sizeof(buffer));
        len = recv(connfd, buffer + sum, sizeof(buffer) - 1, 0);
        if (len <= 0) {
          printf("client closed connection\n");
          break;
        } else {
          printf("recv from client: %s", buffer);
          send(connfd, buffer, strlen(buffer), 0);
        }
      }
    }
  }
  close(sockfd);
  return 0;
}