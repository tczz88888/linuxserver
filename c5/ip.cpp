#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <arpa/inet.h>

int main(){
    in_addr ip;
    ip.s_addr=0x01020304;
    char *addr1=inet_ntoa(ip);
    ip.s_addr=0x04030201;
    char *addr2=inet_ntoa(ip);
    printf("%s\n",addr1);
    printf("%s\n",addr2);
    //inet_ntoa将整数四字节的ip地址转成网络序的点分十进制字符串所以是反的，并且不可重入，返回的是一个静态变量的地址
    ip.s_addr=inet_addr(addr2);
    //inet_addr将网络序的点分十进制字符串转成整数四字节所以是反的，失败返回INADDR_NONE
    printf("%x\n",ip.s_addr);
    int ans=inet_aton(addr2,&ip);
    //inet_aton将网络序的点分十进制字符串转成整数四字节所以是反的,并且存到第二个参数指针指向的变量，成功返回1失败返回0
    printf("%d %x\n",ans,ip.s_addr);
    ip.s_addr=0;
    // 我觉得下面这俩更实用一点，ipv4和ipv6都能转
    //点分十进制字符串转整数，大端转小端，成功返回1失败返回0并且设置errno
    int success=inet_pton(AF_INET, addr2 , &ip.s_addr);
    printf("%d %x\n",success,ip.s_addr);

    //整数转点分十进制字符串，小端转大端，成功返回点分十进制字符串地址，失败返回NULL并且设置errno
    const char *str=inet_ntop(AF_INET, &ip.s_addr, addr2, INET_ADDRSTRLEN);
    printf("%p\n",addr2);
    printf("%p\n",str);

    return 0;
}