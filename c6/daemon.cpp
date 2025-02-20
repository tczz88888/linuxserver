#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
bool flag=1;
void handle(int sig){
    flag=0;
}

int main(){
    int pid_t=fork();
    if(pid_t!=0) exit(0);
    int sid=setsid();
    pid_t=fork();
    if(pid_t!=0) exit(0);
    struct sigaction sa;
    sa.sa_flags=0;
    sa.sa_handler=&handle;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    chdir("/");
    umask(0);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    int fd=open("/home/lyk/linuxstduy/linuxserver/c6/1.txt",O_WRONLY|O_APPEND);
    while(flag){
        write(fd, "123\n", 4);
        sleep(1);
    }
    return 0;
}