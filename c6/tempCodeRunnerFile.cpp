

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
    splice(STDIN_FILENO, NULL, fd_std[1], NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);
    tee(fd_std[0], fd_file[1], 12345, SPLICE_F_NONBLOCK);//只能用于管道
   // tee(fd_std[0], fd_file2[1], 12345, SPLICE_F_NONBLOCK);
    splice(fd_file[0], NULL, filefd, NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);
    splice(fd_file[0], NULL, filefd2, NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);
    splice(fd_std[0], NULL, STDOUT_FILENO, NULL, 12345, SPLICE_F_MORE|SPLICE_F_MOVE);
    return 0;
}