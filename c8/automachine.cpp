/*
目标：使用有限状态自动机解析http请求行和请求头，请求头只支持解析get方法和host，最后要返回这个请求是否合法
*/
#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#define BUFFER_SIZE 4096
char wbuffer[BUFFER_SIZE];
char rbuffer[BUFFER_SIZE];
/*
记录当前的状态在解析请求行，请求头
*/
enum CHECK_STATE { CHECK_STATE_ERQUESTLINE, CHECK_STATE_HEADER };
/*
记录当前行的解析结果行错误，行不完整需要继续读取数据，行合法
*/
enum LINE_STATE { LINE_BAD, LINE_OPEN, LINE_OK };
/*
http请求的处理结果，请求不完整需要继续读取数据，请求不合法，请求合法，客户权限不够，服务器内部错误，客户端连接关闭
*/
enum HTTP_CODE {
  NO_REQUEST,
  BAD_REQUEST,
  GET_REQUEST,
  FORBIDDEN_REQUEST,
  INTERNAL_ERROR,
  CLOSED_CONNECTION
};
/*
为了简化场景，就不给客户端发送响应报文了而是请求成功和失败各发送一句话
*/
static const char *szret[] = {"I get a correct result\n", "Something wrong\n"};
/*
解析一行数据
*/
LINE_STATE parse_line(char *buffer, int &checked_index, int &read_index) {//buffer存的是整个http请求,checked_index表示已经处理的字符数量，read_index表示共有多少数据
  char temp;//记录当前解析到的字符
  for (; checked_index < read_index; ++checked_index) {
    temp=buffer[checked_index];
    /*
    只有当读取到\r或者\n时需要判断
    1.如果当前是\r
    若下一个没了，则需要继续读取
    若下一个是\n，则读取到了完整的一行
    若下一个是其他，则行不合法
    2.如果当前是\n
    如果前一个是\r，则是合法的一行
    否则都是不合法
    3.都不是则继续读
    
    还要将\r\n改成\0\0，方便后续解析
    */
    if(temp=='\r'){
        if(checked_index+1>=read_index) return LINE_OPEN;
        else if(buffer[checked_index+1]=='\n'){
            buffer[checked_index++]='\0';
            buffer[checked_index++]='\0';
            return LINE_OK;
        }
        else return LINE_BAD;
    }
    else if(temp=='\n'){
        if(checked_index>=1&&buffer[checked_index-1]=='\r'){
            buffer[checked_index-1]='\0';
            buffer[checked_index++]='\0';
            return LINE_OK;
        }
        else return LINE_BAD;
    }
  }
  return LINE_OPEN;
}

HTTP_CODE parse_requestLine(char *buffer,CHECK_STATE &checkstate){//正确格式是GET URL HTTP/1.1
    char *url=strpbrk(buffer, " \t");//没有空格或者\t一定不合法
    if(!url){
        return BAD_REQUEST;
    }
    *url++='\0';//相当于将字符串分割，方便后面操作
    char *method=buffer;
    if(strcasecmp(method, "GET")==0){//只支持get方法
        printf("method is get\n");
    }
    else return BAD_REQUEST;

    url+=strspn(url, " \t");//避免有多余的空格和\t，后面的version同理
    char *version=strpbrk(url, " \t");
    if(!version){
        return BAD_REQUEST;
    }
    *version++='\0';//分割
    version+=strspn(version, " \t");
    if(strcasecmp(version, "HTTP/1.1")!=0){
        return BAD_REQUEST;
    }
    if(strncasecmp(url, "http://",7)==0){
        *url+=7;
        url=strchr(url, '/');
    }
    if(!url||url[0]!='/') return BAD_REQUEST;
    printf("The request url is %s",url);
    checkstate=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE parse_requestHead(char *temp){//为了方便只解析出来host
    if(temp[0]=='\0'){//说明是空行
        return GET_REQUEST;
    }
    else if(strncasecmp(temp, "Host:",5)==0){//解析host
        temp+=5;
        temp+=strspn(temp, " \t");
        printf("Host is %s\n",temp);
    }
    else{//其他的不做处理
        printf("To be continue...\n");
    }
    return NO_REQUEST;//继续解析
}

HTTP_CODE parse_content(char *buffer,int &checked_idx,int &read_idx,int &start_line,CHECK_STATE &checkstate){
    LINE_STATE linestatus=LINE_OK;//行状态
    HTTP_CODE retcode=NO_REQUEST;//请求状态
    while ((linestatus=parse_line(buffer, checked_idx, read_idx))==LINE_OK) {//如果一行合法则处理
        char *temp=buffer+start_line;//行的起始位置
        start_line=checked_idx;//下一行的起始位置
        switch (checked_idx) {//判断当前的解析状态
            case(CHECK_STATE_ERQUESTLINE)://解析请求行
            {
                retcode=parse_requestLine(temp, checkstate);
                if(retcode==BAD_REQUEST) return BAD_REQUEST;
                break;
            }
            case(CHECK_STATE_HEADER)://解析请求头
            {
                retcode=parse_requestHead(temp);
                if(retcode==BAD_REQUEST) return BAD_REQUEST;
                else if(retcode==GET_REQUEST) return GET_REQUEST;
                break;
            }
        }
    }
    if(linestatus==LINE_OPEN){//如果行需要继续读数据则返回请求不完整，继续读取
        return NO_REQUEST;
    }//如果行不合法则请求不合法
    else return BAD_REQUEST;
}

int main(int argc ,char *argv[]) { 
    // int fd=open("test.txt", O_RDWR);
    // int sz=read(fd,rbuffer,BUFFER_SIZE-1);
    // printf("%d\n",sz);
    // int cnt=0;
    // for(int i=0;i<sz;i++){
    //     if(rbuffer[i]=='\n'){
    //         buffer[cnt++]='\r';
    //         buffer[cnt++]='\n';
    //     }
    //     else buffer[cnt++]=rbuffer[i];
    // }
    // int start_index=0;
    // int checked_index=0;
    // int read_index=strlen(buffer);
    // while(checked_index<read_index){
    //     start_index=checked_index;
    //     LINE_STATE ret=parse_line(buffer, checked_index, read_index);
    //     if(ret==LINE_OK){
    //         puts("ok");
    //         parse_requestLine(buffer);
    //     }
    //     else printf("%d\n",ret);
    // }
    if(argc<3){
        printf("usage=: %s ip_address port",basename(argv[0]));
        return -1;
    }
    const char *ip=argv[1];
    int port=atoi(argv[2]);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_port=htons(port);
    server_addr.sin_family=AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    int sockfd=socket(PF_INET, SOCK_STREAM , 0);
    int ret=bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    assert(sockfd!=-1);
    listen(sockfd, 5);
    struct sockaddr_in client_addr;
    socklen_t len;
    int connfd=accept(sockfd, (struct sockaddr *)&client_addr, &len);
    if(connfd==-1){
        printf("connect failed\n");
    }
    else{
        int read_idx=0,checked_idx=0,start_line=0;
        CHECK_STATE checkstate=CHECK_STATE_HEADER;
        while(1){//从客户端读取请求信息
            int read_byte=recv(connfd, rbuffer+read_idx, BUFFER_SIZE-read_idx, 0);
            if(read_byte==-1)
            {
                printf("read failed\n");
                break;
            }
            else if(read_byte==0)
            {
                printf("remote client has close the connection\n");
            }
            else{
                read_idx+=read_byte;
                HTTP_CODE retcode=parse_content(rbuffer, checked_idx, read_idx, start_line, checkstate);
                if(retcode==GET_REQUEST){//请求合法
                    send(connfd, szret[0], sizeof(szret[0]), 0);
                    break;
                }
                else if(retcode==NO_REQUEST){//还需要读数据
                    continue;
                }
                else{//请求不合法
                    send(connfd, szret[1], sizeof(szret[1]), 0);
                    break;
                }
            }
        }
    }
    close(connfd);
    close(sockfd);
    return 0;
}