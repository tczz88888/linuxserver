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
const int MAX_USER = 2;
const int MAX_FD = 150;

int num[MAX_FD], cnt = 0;
pollfd events[MAX_FD];
class User {
public:
  sockaddr_in addr;
  char *wbuffer;
  char rbuffer[BUFFER_SIZE];
};
User *user;
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
  cnt = 0;
  user=new User[MAX_USER+1];
  while (1) {
    int ret = poll(events, cnt + 1, -1);
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

          events[cnt].fd = connfd;
          events[cnt].events = POLLIN | POLLERR | POLLRDHUP;
          events[cnt].revents = 0;

          setNoBlocking(connfd);
          printf("come a new user %d,now have %d users\n", connfd,cnt);
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
        } else if (events[i].revents & POLLRDHUP) {
          printf("hup a client left %d\n",events[i].fd);
          close(events[i].fd); 
          user[events[i].fd] = user[events[cnt].fd];
          events[i] = events[cnt];
          --i;
          --cnt;
          
        } else if (events[i].revents & POLLIN) {
          int connfd = events[i].fd;
          memset(user[connfd].rbuffer, '\0', BUFFER_SIZE);
          ret = recv(connfd, user[connfd].rbuffer, BUFFER_SIZE - 1, 0);
          if (ret < 0) {
            if (errno != EAGAIN) {
            printf("error a client left %d\n",events[i].fd);
              close(events[i].fd);
              user[events[i].fd] = user[events[cnt].fd];
              events[i] = events[cnt];
              --i;
              --cnt;
            }
          }
          else if(ret==0){

          }
          else{
            for(int j=1;j<=cnt;j++){
                if(events[j].fd!=connfd){
                    user[events[j].fd].wbuffer=user[connfd].rbuffer;
                    events[j].events|=~POLLIN;
                    events[j].events|=POLLOUT;
                }
            }
            printf("recv from client: %s\n",user[connfd].rbuffer);
          }
        }
        else if(events[i].revents&POLLOUT){
            int connfd=events[i].fd;
            if(!user[connfd].wbuffer) {
              continue;
            }
            ret=send(connfd, user[connfd].wbuffer, strlen(user[connfd].wbuffer), 0);
            printf("send to%d\n",connfd);
            user[connfd].wbuffer=NULL;
            events[i].events|=~POLLOUT;
            events[i].events|=POLLIN;
        }
      }
    }
  }
  delete [] user;
  return 0;
}