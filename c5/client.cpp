


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
    }
    else{
        const char *oob_data="123"; 
        const char *normal_data="abc"; 
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
        send(sockfd,normal_data, strlen(normal_data), 0);
    }
    close(sockfd);
    return 0;
}