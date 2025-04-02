#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <bits/types/struct_iovec.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <sys/wait.h> 
#include <arpa/inet.h>
class http_conn
{
    public:
    /*文件名最大长度*/
    static const int FILENAME_LEN=200;
    /*读缓冲区大小*/
    static const int READ_BUFFER_SIZE=2048;
    /*写缓冲区大小*/
    static const int WRITE_BUFFER_SIZE=1024;

    
    /*http请求方法，目前代码只支持get*/
    enum METHOD{
        GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH
    };
    /*解析请求时，主状态机可能的状态*/
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE=0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    /*服务器处理HTTP请求的可能结果*/
    enum HTTP_CODE{
        NO_REQUEST=0,GET_REQQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSE_CONNECTION
    };
    /*行的读取状态*/
    enum LINE_STATUS{
        LINE_OK=0,LINE_BAD,LINE_OPEN
    };


    
    public:
        /*初始化新接受的连接*/
        void init(int sockfd,const sockaddr_in& addr);
        /*关闭连接*/
        void close_conn(bool real_close=true);
        /*处理客户请求*/
        void process();
        /*非阻塞读*/
        bool read();
        /*非阻塞写*/
        bool write();

    private:
        /*初始化连接*/
        void init();
        /*解析HTTP请求*/
        HTTP_CODE process_read();
        /*填充HTTP应答*/
        bool process_write(HTTP_CODE ret);

        /*以下被process_read调用，以分析HTTP请求*/
        HTTP_CODE parse_request_line(char *text);
        HTTP_CODE parse_headers(char *text);
        HTTP_CODE parse_content(char *text);
        HTTP_CODE do_request();

        char* get_line() {return m_read_buf+m_start_line;}
        LINE_STATUS parse_line();


        /*以下被process_write调用，以填充HTTP应答*/
        void unmap();
        bool add_response(const char* format,...);
        bool add_content(const char * content);
        bool add_status_line(int status,const char *title);
        bool add_headers(int content_len);
        bool add_content_length(int content_length);
        bool add_linger();
        bool add_blank_line();

    public:
        /*所有socket上的事件都注册到一个epollfd中*/
        static int m_epollfd;
        /*统计用户数量*/
        static int m_user_count;   
        static int m_done;
    
    private:
        /*该HTTP连接的socket和对方的socket地址*/
        int m_sockfd;
        sockaddr_in m_address;

        /*读缓冲区*/
        char m_read_buf[READ_BUFFER_SIZE];
        /*标记读缓区中已经读到的下一个字节的位置*/
        int m_read_idx;
        /*当前正在分析的字符在读缓冲区中的位置*/
        int m_checked_idx;
        /*当前正在解析的行的起始位置*/
        int m_start_line;
        /*写缓冲区*/
        char m_write_buf[WRITE_BUFFER_SIZE];
        /*写缓冲区中待发送的字节数*/
        int m_write_idx;

        /*当前状态机的状态*/
        CHECK_STATE m_check_state;
        /*请求方法*/
        METHOD m_method;

        /*客户请求成功的目标文件的完整路径，等于doc_root+m_url,doc_root是网站根目录*/
        char m_read_file[FILENAME_LEN];
        /*客户请求的目标文件的文件名*/
        char *m_url;
        /*HTTP协议版本号，我们仅支持HTTP/1.1*/
        char *m_version;
        /*主机名*/
        char *m_host;
        /*HTTP请求的消息体的长度*/
        int m_content_length;
        /*HTTP请求是否要求保持连接*/
        bool m_linger;

        /*客户请求的目标文件被mmap到内存中的起始位置*/
        char *m_file_address;
        /*目标文件的状态，可以判断文件是否存在，是否为目录，是否可读，并获取文件大小等信息*/
        struct stat m_file_stat;
        /*采用writev来写，要指定多个内存块*/
        iovec m_iv[2];
        int m_iv_count;
};


#endif