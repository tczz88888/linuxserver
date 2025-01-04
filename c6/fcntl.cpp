


#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
int setnoblocking(int fd){
    int old_option=fcntl(fd, F_GETFL);//获得文件描述符旧状态
    int new_option=old_option|O_NONBLOCK;//设置非阻塞
    fcntl(fd, F_SETFL,new_option);//设置新状态
    return old_option;//返回旧状态以便之后恢复
}

int main(){
    char pos[1024];
    getcwd(pos, 1024-1);
    printf("%s\n",pos);
    return 0;
}