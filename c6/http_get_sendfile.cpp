#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_iovec.h>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include<sys/sendfile.h>

static bool stop = false; //如果捕获到SIGTERM信号就结束监听
static void handle_term(int sig) { stop = true; }
const int BUF_SIZE = 4096;
const char* status_line[2]={"200 ok","500 Internal server error"};

int main(int argc, char *argv[]) {
  signal(SIGTERM, handle_term);
  printf("number=%d\n", argc);
  if (argc <= 2) {
    printf("usage=: %s ip_address port_number filename", basename(argv[0]));
    return -1;
  }
  const char *ip = argv[1];
  int port = atoi(argv[2]);
  const char *filename = argv[3];
  // int backlog=atoi(argv[3]);//读取参数，分别是ip，端口号，监听队列最大长度
  int sockfd = socket(PF_INET, SOCK_STREAM, 0); //创建一个socket
  // int opt = 1;
	// setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const void*)&opt,sizeof(opt));
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

  int connfd = accept(sockfd, (struct sockaddr *)&client, &client_len);
  if (connfd == -1) {
    printf("connect failed %d\n", errno);
  } else {

    char header_buf[BUF_SIZE]; //存http应答的状态行，头部字段和一个空行缓存区
    memset(header_buf, '\0', BUF_SIZE);

    //用于存放目标文件的内容的缓存
    char *file_buf;

    //获取目标文件的属性，比如是否为目录，文件大小等
    struct stat file_stat;

    //记录目标文件是否是有效文件
    bool valid = true;

    //记录header_buf使用了多少空间
    int len;

    if (stat(filename, &file_stat) < 0) //找不到目标文件{
    {
      valid = false;
    }
    else{
        if(S_ISDIR(file_stat.st_mode)){//目标文件是个目录
            valid=false;
        }
        else if(file_stat.st_mode&S_IROTH){//当前用户有读取目标文件的权限
            //将目标文件的内容缓存到file_buf中
            int fd=open(filename,O_RDONLY);
            int size=file_stat.st_size;
            file_buf=(char *)malloc(size+1);
            memset(file_buf, '\0', file_stat.st_size+1);
            if(read(fd,file_buf,file_stat.st_size)<0){
                valid=false;
            }
        }
        else{
            valid=false;

        }
    }
    if(valid){
            // ret=snprintf(header_buf, BUF_SIZE-1, "%s %s\r\n","HTTP/1.1",status_line[0]);
            // len+=ret;
            // ret=snprintf(header_buf+len, BUF_SIZE-1-len, "Content_Length: %ld\r\n",file_stat.st_size);
            // len+=ret;
            // ret=snprintf(header_buf+len, BUF_SIZE-1-len, "%s","\r\n");
            // //使用writev将两块内存header_buf和file_buf的内容一起写到connfd中
            // struct iovec iv[2];
            // iv[0].iov_base=header_buf;
            // iv[0].iov_len=strlen(header_buf);
            // iv[1].iov_base=file_buf;
            // iv[1].iov_len=file_stat.st_size;
            // auto t1=clock();
            // ret=writev(connfd, iv, 2);
            // auto t2=clock();
            // printf("%lfms\n",1.0*(t2-t1)/CLOCKS_PER_SEC*1000);

            /*
            ssize_t  sendfile(int  out_fd,  int in_fd, off_t *offset,
       size_t count);
            out_fd是需要写入的文件描述符，in_fd是读取文件的文件描述符，offset表示偏移量，从第offset个字符开始发送，最后offset指向文件末尾，count表示需要发送的大小
            成功返回发送的字节数,失败返回-1
            */
            // int fd=open(filename,O_RDONLY);
            // auto t1=clock();
            // off_t offset=5;
            // ret=sendfile(connfd, fd,&offset, file_stat.st_size+5);
            // auto t2=clock();
            // printf("%ld %ld %d\n",(t2-t1),offset,ret);

        }
        else{
          //目标文件无效，通知客户端内部错误
            ret=snprintf(header_buf, BUF_SIZE-1, "%s %s\r\n","HTTP/1.1",status_line[1]);
            len+=ret;
            ret=snprintf(header_buf+len, BUF_SIZE-1-len, "%s","\r\n");
            send(connfd,header_buf,strlen(header_buf),0);
        }
    sleep(1);
    close(connfd);
  }
  close(sockfd);
  return 0;
}