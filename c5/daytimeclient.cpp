#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <cstdio>

int main(int argc,char *argv[]){
    // const char *hostname="localhost";
    // struct hostent* hostinfo=gethostbyname(hostname);//获取目的主机的信息
    // assert(hostinfo);
    // struct servent* servinfo=getservbyname("daytime", "tcp");
    // assert(servinfo);
    // printf("daytime port is %d\n",ntohs(servinfo->s_port));
    struct addrinfo hints;
    hints.ai_socktype=SOCK_STREAM;
    bzero(&hints, sizeof(hints));
    struct addrinfo *res;
    getaddrinfo("localhost", "daytime", &hints, &res);
    char buff[1000];
    printf("localhost ip=%s,daytime port=%d\n",inet_ntop(AF_INET,&(((struct sockaddr_in*)res->ai_addr)->sin_addr) ,buff, INET_ADDRSTRLEN),ntohs(((struct sockaddr_in*)res->ai_addr)->sin_port));
    int sokcfd=socket(AF_INET, SOCK_STREAM, 0);
    int ret=connect(sokcfd, res->ai_addr, res->ai_addrlen);
    if(ret==-1)
    perror("Error");
    if(ret!=-1){
        const int BUF_SIZE=5120;
        char buf[BUF_SIZE];
        memset(buf, '\0', BUF_SIZE);
        ret=read(sokcfd, buf, BUF_SIZE);
        printf("time is %s",buf);
    }
    return 0;
}