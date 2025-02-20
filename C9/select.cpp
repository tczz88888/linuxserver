/*
#include <sys/select.h>

int select(int nfds, fd_set *_Nullable restrict readfds,
                  fd_set *_Nullable restrict writefds,
                  fd_set *_Nullable restrict exceptfds,
                  struct timeval *_Nullable restrict timeout);
参数：nfds：监听的文件描述符总数，一般被设置为select监听的所有文件描述符中的最大值+1
     readfds,writefds和exceptfds分别指向可读，可写和异常事件对应的文件描述符集合，向这三个参数传入自己要用的文件描述符
     select返回时会修改他们，来通知那些文件描述符已经准备就绪
     timeout就是超时时间，秒数，微妙数

返回值：成功时返回就绪的文件描述符总数，超时时间内没有任何文件描述符就绪时返回0，失败返回-1并且设置errno，在select等待期间，程序
       收到信号，则select立即返回-1，并且设置errno为EINTR

fd_set是一个整形数组，每一个元素的每一位标记一个文件描述符，所以需要用位运算，以下函数方便我们操作

void FD_CLR(int fd, fd_set *set);清除fd_set的fd位
int  FD_ISSET(int fd, fd_set *set);测试fd_set的fd位是否被设置
void FD_SET(int fd, fd_set *set);设置fd_set的fd位
void FD_ZERO(fd_set *set);清除fd_set的所有位


*/