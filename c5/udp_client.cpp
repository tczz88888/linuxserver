

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
  if (argc <= 2) {
    printf("usage=: %s ip_address port_number", basename(argv[0]));
    return -1;
  }
  const char *ip = argv[1];
  int port = atoi(argv[2]);
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  inet_pton(AF_INET, ip, &address.sin_addr);
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  assert(sockfd >= 0);
  const char *normal_data1 = "发给我";
  const char *normal_data2 = "已接收";
  char buf[1024];
  socklen_t address_len=sizeof(address);
  sendto(sockfd, normal_data1, strlen(normal_data1), 0,
         (struct sockaddr *)&address, sizeof(address));

  
  recvfrom(sockfd, buf, 1023, 0, (struct sockaddr *)&address, &address_len);
  printf("recv from server data:%s\n",buf);

  sendto(sockfd, normal_data2, strlen(normal_data2), 0,
         (struct sockaddr *)&address, sizeof(address));

  close(sockfd);
  return 0;
}