
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
/*
int pipe(int pipefd[2]);
成功返回0，并且将文件描述符传入pipefd中，fd[0]只能读，fd[1]只能写，失败返回-1，设置errno
*/

int main() {
  int pipefd[2];
  int res = pipe(pipefd);
  assert(res == 0);
  char wbuf[1024];
  std::cin >> wbuf;
  write(pipefd[1], wbuf, strlen(wbuf));
  int cnt = 3;
  struct iovec irbuf[cnt];
  char *rbuf[cnt];
    memset(rbuf, '\0', sizeof(rbuf));
  for (int i = 0; i < cnt; i++) {
    rbuf[i]=(char *)malloc(4*sizeof(char));
    irbuf[i].iov_base = rbuf[i];
    irbuf[i].iov_len = 4;
  }
  ssize_t sz = readv(pipefd[0], irbuf, cnt);
  for (int i = 0; i < cnt; i++) {
    std::cout<<(char *)irbuf[i].iov_base<<' '<<irbuf[i].iov_len<<' '<<strlen((char *)irbuf[i].iov_base)<<'\n';
  }
  
  return 0;
}