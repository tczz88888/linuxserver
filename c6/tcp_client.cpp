    


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
#include <iostream>
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
    if(connect(sockfd, (struct sockaddr*)&address, sizeof(address))<0){
        printf("connect failed\n");
        perror(strerror(errno));
    }
    else{
        char data[10000]; 
        memset(data, '\0', 10000);
        read(sockfd, data, sizeof(data));
        std::cout<<data<<'\n';
    }
    close(sockfd);
    return 0;
}