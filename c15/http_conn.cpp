#include "http_conn.h"
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

/*HTTP响应的状态信息*/
const char* ok_200_title="OK";
const char* error_400_title="Bad Request";
const char* error_400_form="Your Request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title="Forbidden";
const char* error_403_form="You do not have permission to get file from this server.\n";
const char* error_404_title="Not Found";
const char* error_404_form="The request file was not found on this server.\n";
const char* error_500_title="Internet Error";
const char* error_500_form="There was an unusual problem serving the requested file.\n";

/*网站的根目录*/
const char* doc_root="/var/www/html";

int setnoblocking(int fd){
    int oldoption=fcntl(fd, F_GETFL);
    int newoption=oldoption|O_NONBLOCK;
    fcntl(fd, F_SETFL,newoption);
    return oldoption;
}

void addfd(int epollfd,int fd,bool one_shot){
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    if(one_shot){
        event.events|=EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd,&event);
    setnoblocking(fd);
}

void removefd(int epollfd,int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void modfd(int epollfd,int fd,int ev){
    epoll_event event;
    event.data.fd=fd;
    event.events=ev|EPOLLIN|EPOLLET|EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd,&event);
}

int http_conn::m_user_count=0;
int http_conn::m_epollfd=-1;

void http_conn::close_conn(bool real_close){
    if(real_close&&m_sockfd!=-1){//关闭与客户端的连接，并且计数-1
        removefd(m_epollfd, m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
}

void http_conn::init(int sockfd,const sockaddr_in& addr){
    m_sockfd=sockfd;
    m_address=addr;
    /*下面两行是为了避免time-wait,仅调试时使用*/
    int reuse=1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}


/*初始化一些参数*/
void http_conn::init(){
    m_check_state=CHECK_STATE_REQUESTLINE;
    m_linger=false;

    m_method=GET;
    m_url=nullptr;
    m_version=nullptr;
    m_content_length=0;
    m_host=nullptr;
    m_start_line=0;
    m_checked_idx=0;
    m_read_idx=0;
    m_write_idx=0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_read_file, '\0', FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for(;m_checked_idx<m_read_idx;++m_checked_idx){
        temp=m_read_buf[m_checked_idx];
        if(temp=='\r'){
            if((m_checked_idx+1)==m_read_idx){
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx+1]=='\n'){
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n'){
            if((m_checked_idx>1)&&(m_read_buf[m_checked_idx-1]=='\r')){
                m_read_buf[m_checked_idx-1]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//循环读取客户端数据，直到无数据可读或者对方关闭
bool http_conn::read(){
    if(m_read_idx>=READ_BUFFER_SIZE){
        return false;
    }
    int byte_read=0;
    while(1){
        byte_read=recv(m_sockfd, m_read_buf, READ_BUFFER_SIZE-m_read_idx, 0);
        if(byte_read==1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                break;
            }
            return false;
        }
        else if(byte_read==0){
            return false;
        }
        m_read_idx+=byte_read;
    }
    return true;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    m_url=strpbrk(text, " \t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++='\0';
    char *method=text;
    if(strcasecmp(method, "GET")==0){
        m_method=GET;
    }
    else{
        return BAD_REQUEST;
    }
    m_url=m_url+strspn(m_url, " \t");
    m_version=strpbrk(m_url, " \0");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++='\0';
    m_version+=strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1")!=0){
        return BAD_REQUEST;
    }
    if(strncasecmp(m_url, "http://", 7)==0){
        m_url+=7;
        m_url=strchr(m_url, '/');
    }
    if(!m_url||m_url[0]!='/'){
        return BAD_REQUEST;
    }
    m_check_state=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/*解析HTTP请求的一个头部信息*/
http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    /*空行标志请求头结束*/
    if(text[0]=='\0'){
        /*如果有消息体，则继续读m_content_length字节的消息体，自动机状态转移到CHECK_STATE_CONTENT状态*/
        if(m_content_length!=0){
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQQUEST;
    }
    /*处理Connection头部字段*/
    else if(strncasecmp(text, "Connection:", 11)==0){
        text+=11;
        text+=strspn(text, " \t");
        if(strcasecmp(text, "keep-alive")==0){
            m_linger=true;
        }
    }
    /*处理Content-Length字段*/
    else if(strncasecmp(text, "Content-Length:", 15)==0){
        text+=15;
        text+=strspn(text, " \t");
        m_content_length=atoi(text);
    }
    /*处理Host字段*/
    else if(strncasecmp(text, "Host:", 5)==0){
        text+=5;
        text+=strspn(text, " \t");
        m_host=text;
    }
    else{
        printf("oop!unknow header %s\n",text);
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char* text){
    if(m_read_idx>=m_checked_idx+m_content_length){
        text[m_content_length]='\0';
        return GET_REQQUEST;
    }
    return NO_REQUEST;//还没读完
}


http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=nullptr;
    while((m_check_state==CHECK_STATE_CONTENT&&line_status==LINE_OK)||((line_status=parse_line())=LINE_OK)){
        text=http_conn::get_line();
        m_start_line=m_checked_idx;
        printf("get 1 http line %s\n",text);
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE:{
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:{
                ret=parse_headers(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }
                if(ret==GET_REQQUEST){
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret=parse_content(text);
                if(ret==GET_REQQUEST){
                    return do_request();
                }
                line_status=LINE_OPEN;
                break;
            }
            default:{
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_read_file,doc_root);
    int len=strlen(doc_root);
    strncpy(m_read_file+len, m_url, FILENAME_LEN-len-1);
    if(stat(m_read_file,&m_file_stat)<0){
        return NO_RESOURCE;
    }
    if(!((m_file_stat.st_mode)&S_IROTH)){
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }
    int fd=open(m_read_file, O_RDONLY);
    m_file_address=(char*)mmap(nullptr, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

