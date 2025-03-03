#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
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
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
const int BUFFER_SIZE = 1000;
const int MAX_USER = 150;
const int MAX_FD = 150;

int num[MAX_FD], cnt = 0;
pollfd events[MAX_FD];
class User {
public:
  sockaddr_in addr;
  char *buffer;
  ~User() { delete buffer; }
};
User user[MAX_USER];
int setNoBlocking(int fd) {
  int oldop = fcntl(fd, F_GETFL);
  int newop = oldop | O_NONBLOCK;
  fcntl(fd, F_SETFL, newop);
  return oldop;
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    printf("usage %s ip_address port", basename(argv[0]));
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

  for (int i = 0; i < MAX_FD; i++) {
    events[i].fd = -1;
    events[i].events = 0;
  }
  events[0].fd = listenfd;
  events[0].events = POLLIN;
  events[0].revents = 0;
  cnt = 1;
  while (1) {
    int ret = poll(events, cnt + 1, 1);
    if (ret < 0) {
      printf("poll failure\n");
      break;
    } else {
      for (int i = 0; i < cnt + 1; i++) {
        if (events[i].fd == listenfd && (events[i].revents & POLLIN)) {
          sockaddr_in client_addr;
          socklen_t len;
          int connfd = accept(listenfd, (sockaddr *)&client_addr, &len);
          if (connfd < 0) {
            printf("error is %d", errno);
          }
          if (cnt >= MAX_USER) {
            const char *info = "to many users";
            printf("%s\n", info);
            send(connfd, info, strlen(info), 0);
            close(connfd);
            continue;
          }
          cnt++;
          user[connfd].addr = client_addr;
          user[connfd].buffer = new char[BUFFER_SIZE];

          events[cnt].fd = connfd;
          events[cnt].events = POLLIN | POLLHUP | POLLERR;
          events[cnt].revents = 0;

          setNoBlocking(connfd);
          printf("come a new user,now have %d users\n", cnt);
        } else if (events[i].revents & POLLERR) {
          printf("get error from fd %d\n", events[i].fd);
          char errors[100];
          memset(errors, '\0', sizeof(errors));
          socklen_t len;
          if (getsockopt(events[i].fd, SOL_SOCKET, SO_ERROR, &errors, &len) <
              0) {
            printf("get socket option failed\n");
          }
          continue;
        } else if (events[i].revents & POLLHUP) {
          user[events[i].fd] = user[cnt];
          close(events[i].fd);
          --cnt;
          events[i] = events[cnt];
          --i;
          printf("a client left\n");
        } else if (events[i].revents & POLLIN) {
          int connfd = events[i].fd;
          memset(user[connfd].buffer, '\0', BUFFER_SIZE);
          ret = recv(connfd, user[connfd].buffer, BUFFER_SIZE - 1, 0);
          if (ret < 0) {
            if (errno != EAGAIN) {
              user[events[i].fd] = user[cnt];
              close(events[i].fd);
              --cnt;
              events[i] = events[cnt];
              --i;
              printf("a client left\n");
            }
          }
          else if(ret==0){

          }
          else{

          }
        }
        else if(events[i].revents&POLLOUT){

        }
      }
    }
  }
  return 0;
}