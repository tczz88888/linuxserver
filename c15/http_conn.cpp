#include "http_conn.h"
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstdarg>
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
#include <sys/uio.h>

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
int http_conn::m_done=0;

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
    printf("strat read data from %d............\n",m_sockfd);
    if(m_read_idx>=READ_BUFFER_SIZE){
        return false;
    }
    int byte_read=0;
    while(1){
        byte_read=recv(m_sockfd, m_read_buf, READ_BUFFER_SIZE-m_read_idx, 0);
        if(byte_read==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                break;
            }
            return false;
        }
        else if(byte_read==0){
            return false;
        }
        m_read_idx+=byte_read;
        printf("data is %s",m_read_buf);
    }
    printf("end read data from %d............\n",m_sockfd);
    return true;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    printf("start parse request line\n");
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
    printf("start parse request header\n");
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
    printf("start parse content\n");
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
    while((m_check_state==CHECK_STATE_CONTENT&&line_status==LINE_OK)||((line_status=parse_line())==LINE_OK)){
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


/*得到一个完整，正确的请求之后，检查get的文件是否有效，有效则映射到m_file_address中*/
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


void http_conn::unmap(){
    if(m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address=0;
    }
}

/*写HTTP响应*/
bool http_conn::write(){
    int temp=0;
    int byte_to_send=m_write_idx+m_file_stat.st_size;
    if(byte_to_send==0){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while(1){
        int temp=writev(m_sockfd, m_iv, m_iv_count);
        if(temp<-1){
            if(errno==EAGAIN){//读缓存没用空间了
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        byte_to_send-=temp;
        if(byte_to_send<=0){
            /*HTTP响应发送成功*/
            ++m_done;
            unmap();
            if(m_linger){
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else{
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

//往写缓存里面写入响应数据,如何从使用va_list写入format数据
bool http_conn::add_response(const char* format,...){
    if(m_write_idx>=WRITE_BUFFER_SIZE){
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len=vsnprintf(m_write_buf+m_write_idx, WRITE_BUFFER_SIZE-1-m_write_idx, format, arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx)){
        return false;
    }
    m_write_idx+=len;
    va_end(arg_list);
    return true;
}

//添加响应头


bool http_conn::add_status_line(int status,const char* title){
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}
bool http_conn::add_headers(int content_len){
    return add_content_length(content_len)&&add_linger()&&add_blank_line();
}

bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length: %d\r\n",content_len);
}

bool http_conn::add_linger(){
    return add_response("Connection: %s\r\n",(m_linger==true)?"keep-alive":"close");
}

bool http_conn::add_blank_line(){
    return add_response("%s","\r\n");
}

bool http_conn::add_content(const char* content){
    return add_response("%s",content);
}

/*根据HTTP请求的结果，决定返回给客户端的数据*/
bool http_conn::process_write(HTTP_CODE ret){
    switch (ret) {
        case INTERNAL_ERROR:{
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form)){
                return false;
            }
            break;
        }
        case BAD_REQUEST:{
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form)){
                return false;
            }
            break;

        }
        case NO_RESOURCE:{
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form)){
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:{
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form)){
                return false;
            }
            break;
        }
        case FILE_REQUEST:{
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size!=0){
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2;
                return true;
            }
            else{
                const char* ok_string="<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string)){
                    return false;
                }
            }
        }
        default:{
            return false;
        }
    }
    m_iv[0].iov_base=m_write_buf;
    m_iv[0].iov_len=m_write_idx;
    m_iv_count=1;
    return true;
}

/*线程执行的函数*/
void http_conn::process(){
    HTTP_CODE read_ret=process_read();
    if(read_ret==NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        printf("request is not incomplete \n");
        return ;
    }
    bool write_ret=process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}