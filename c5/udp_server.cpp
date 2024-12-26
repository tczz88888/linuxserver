#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/*
int socket(int domain,int type,int protocol)
domain:指定底层协议族，如果使用tcp/ip,该参数要设置成PF_INET(ipv4),或者PF_INET6(ipv6),对于unix本地域协议族,应该设置成PF_UNIX
type:tcp用SOCK_STREAM(数据流),udp用SOCK_DGRAM(数据报),2.6.17之后，还可以指定SOCK_NOBLOCK和SOCK_CLOEXEC,分别表示将创建的socket设置非阻塞以及调用fork时，在子进程中关闭该socket
protocol:一般是0

返回值:成功返回文件描述符，失败返回-1并设置errno
 */

/*
int bind(int sockfd, const struct sockaddr *addr,
                socklen_t addrlen);
将addr指向的socket地址分配给sockfd这个文件描述符,addrlen指定长度
bind成功返回0，失败返回-1并且设置errno,有两种常见的1.EACCES,被绑定的地址是受保护的地址，2.EADDRINUS,被绑定的地址正在使用中
*/

/*
int  listen(int sockfd,int backlog)
创建一个监听队列,sockfd指定被监听的socket，backlog设置监听队列的最大长度，超过backlog服务器将不受理新的客户链接,客户端收到ECONNREFUSED
2.2之前backlog是半连接和全连接的总和，2.2之后只是全连接
*/

static bool stop = false; //如果捕获到SIGTERM信号就结束监听
static void handle_term(int sig) { stop = true; }
const int BUF_SIZE = 1024;

int main(int argc, char *argv[]) {
  signal(SIGTERM, handle_term);
  printf("number=%d\n", argc);
  if (argc <= 2) {
    printf("usage=: %s ip_address port_number backlog", basename(argv[0]));
    return -1;
  }
  const char *ip = argv[1];
  int port = atoi(argv[2]);
  // int backlog=atoi(argv[3]);//读取参数，分别是ip，端口号，监听队列最大长度
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0); //创建一个socket
  assert(sockfd >= 0);
  struct sockaddr_in address;                //创建一个sockaddr
  bzero(&address, sizeof(address));          //置0
  address.sin_port = htons(port);            //将host转成net
  address.sin_family = AF_INET;              //指定协议族
  inet_pton(AF_INET, ip, &address.sin_addr); // host转net
  int ret = bind(sockfd, (struct sockaddr *)&address,
                 sizeof(address)); //将sockaddr指向内内容绑定到socket
  assert(sockfd != -1);
  ret = listen(sockfd, 5); //设置监听
  struct sockaddr_in client;
  socklen_t client_len = sizeof(client);

  char buf[BUF_SIZE];
  const char *data="好，我发送";
  memset(buf, '\0', BUF_SIZE);
  ret = recvfrom(sockfd, buf, BUF_SIZE - 1, 0, (struct sockaddr *)&client,
                 &client_len);
  printf("recv from client data:%s\n",buf);


  sendto(sockfd, data, strlen(data), 0, (struct sockaddr *)&client, client_len);

  memset(buf, '\0', BUF_SIZE);
  ret = recvfrom(sockfd, buf, BUF_SIZE - 1, 0, (struct sockaddr *)&client,
                 &client_len);
  printf("recv from client data:%s\n",buf);

  close(sockfd);


  return 0;
}