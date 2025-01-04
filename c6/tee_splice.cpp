

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
int main(){
    int filefd=open("1.txt", O_WRONLY);
    int filefd2=open("2.txt", O_WRONLY);
    int fd_std[2];
    int fd_file[2];
    int fd_file2[2];
    int ret=pipe(fd_std);
    ret=pipe(fd_file);
    ret=pipe(fd_file2);
    splice(STDIN_FILENO, NULL, fd_std[1], NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);//管道才会阻塞，如果用其他文件描述符就直接结束了
    tee(fd_std[0], fd_file[1], 12345, SPLICE_F_NONBLOCK);//只能用于管道，并且不消耗数据
   // tee(fd_std[0], fd_file2[1], 12345, SPLICE_F_NONBLOCK);
    splice(fd_file[0], NULL, filefd, NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);
    //splice(fd_file[0], NULL, filefd2, NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);
    //为什么这样写会一直阻塞呢，是因为splice默认是阻塞的，在上面把fd_file[0]的内容给filefd之后，fd_file[0]就空了，要把fd_file[0]给filefd2,fd_file这个管道就会阻塞
    splice(fd_std[0], NULL, STDOUT_FILENO, NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);
    //这些操作都是零拷贝
    return 0;
}