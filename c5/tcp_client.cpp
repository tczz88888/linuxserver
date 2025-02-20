


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
int main(int argc,char *argv[]){
    if(argc<=2){
        printf("usage=: %s ip_address port_number",basename(argv[0]));
        return -1;
    }
    const char * ip=argv[1];
    int port=atoi(argv[2]);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family=AF_INET;
    address.sin_port=htons(port);
    inet_pton(AF_INET ,ip, &address.sin_addr);
    int sockfd=socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd>=0);
    int sendbuf = 50;
  socklen_t len = sizeof(sendbuf);
  setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
  getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, &len);
  printf("sendbuf=%d\n", sendbuf);
    if(connect(sockfd, (struct sockaddr*)&address, sizeof(address))<0){
        printf("connect failed\n");
    }
    else{
        char data[10000]; 
        memset(data, 'a', 10000);
        send(sockfd, data, 10000, 0);
    }
    close(sockfd);
    return 0;
}