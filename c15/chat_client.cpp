

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
#include <strings.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
char data[4096];
int main(int argc, char *argv[]) {
  if (argc <= 2) {
    printf("usage=: %s ip_address port_number", basename(argv[0]));
    return -1;
  }
  while (1) {
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    int ret = connect(sockfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    pollfd fds[2];
    // fds[0].fd=0;
    // fds[0].events=POLLIN;
    // fds[0].revents=0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;
    const char *msg = "GET /tmp HTTP/1.1\r\nConnection: keep-alive\r\nHost: "
                      "ts1.cn.mm.bing.net\r\n\r\n";
    send(sockfd, msg, strlen(msg), 0);
    int pfd[2];
    ret = pipe(pfd);
    do {
      int ret = poll(fds, 2, -1);
      if (ret < 0) {
        printf("poll failure\n");
        break;
      }

      if (fds[1].revents & POLLRDHUP) {
        printf("server close connection\n");
        break;
      } else if (fds[1].revents & POLLIN) {
        memset(data, '\0', sizeof(data));
        recv(sockfd, data, sizeof(data) - 1, 0);
        printf("recv from server :%s\n", data);
      }

      // if(fds[0].revents&POLLIN){
      // //    ret=splice(0, NULL,sockfd, NULL, 32768,
      // SPLICE_F_MORE|SPLICE_F_MOVE);

      //     ret=splice(0, NULL,pfd[1], NULL, 32768,
      //     SPLICE_F_MORE|SPLICE_F_MOVE); ret=splice(pfd[0], NULL,sockfd, NULL,
      //     32768, SPLICE_F_MORE|SPLICE_F_MOVE);
      // }
    }while(0);
    close(sockfd);
  }
  return 0;
}