#ifndef LST_TIMER
#define LST_TIMER

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define BUFFER_SIZE 64

class heap_timer;

class
    client_data { // 如何才能向一个链接发送数据呢，肯定要一个sockfd一个buf，然后用util_timer做计时器，并且存一下对应的sockaddr
public:
  sockaddr_in client_addr;
  int sockfd;
  char buf[BUFFER_SIZE];
  heap_timer *timer;
};

class
    heap_timer { // 计时器，需要一个超时时间，指向前驱和后继的指针，一个callback用于到期后的处理，需要一个client才知道通知谁到期了
public:
  time_t expire;
  client_data *user_data;
  void (*cb_func)(client_data *);

  heap_timer(int delay) { expire = time(nullptr) + delay; }
};

class timer_heap {
public:
  timer_heap(int cap) noexcept(false)
      : capcity(cap), cur_size(0) { // 构建一个空的时间堆
    array = new heap_timer *[capcity];
    if (!array) {
      throw std::exception();
    }
    for (int i = 0; i < capcity; i++) {
      array[i] = nullptr;
    }
  }

  timer_heap(int cap, int size, heap_timer **init_array) noexcept(false)
      : capcity(cap), cur_size(size) { // 根据已有的数组构建一个时间堆
    array = new heap_timer *[capcity];
    if (!array) {
      throw std::exception();
    }
    for (int i = 0; i < capcity; i++) {
      array[i] = nullptr;
    }
    if (size != 0) {
      for (int i = 0; i < size; i++) {
        array[i] = init_array[i];
      }
      for (int i = (cur_size - 1) / 2; i >= 0; --i) {
        percolate_down(i);
      }
    }
  }

  ~timer_heap() {
    for (int i = 0; i < capcity; i++) {
      delete array[i];
    }
    delete[] array;
  }

  void add_timer(heap_timer *timer) {
    if (!timer) {
      return;
    }
    if (cur_size >= capcity) {
      resize();
    }
    int hole = cur_size++;
    int parent = 0;
    for (; hole > 0; hole = parent) {
      parent = (hole - 1) / 2;
      if (timer->expire >= array[parent]->expire) {
        break;
      }
      array[hole] = array[parent];
    }
    array[hole] = timer;
    printf("new %ld\n",timer->expire);
  }

  void delete_timer(heap_timer *timer) {
    if (!timer) {
      return;
    }
    timer->cb_func = nullptr;
  }

  void adjust_timer(heap_timer *timer,int delay){
    heap_timer *new_timer=new heap_timer(0);
    time_t cur=time(nullptr);
    new_timer->expire=cur+delay;
    new_timer->cb_func=timer->cb_func;
    new_timer->user_data=timer->user_data;
    delete_timer(timer);
    add_timer(new_timer);
  }

  heap_timer *top() {
    if (empty()) {
      return nullptr;
    }
    return array[0];
  }

  void pop() {
    if (empty()) {
      return;
    }
    if (array[0]) {
      delete array[0];
      array[0] = array[--cur_size];
      percolate_down(0);
    }
  }

  void tick() {
    printf("tick() %ld %d\n",time(nullptr),cur_size);
    heap_timer *temp = array[0];
    time_t cur = time(nullptr);
    while (!empty()) {
      if (!temp) {
        break;
      }
      if (temp->expire > cur) {
        break;
      }
      if (array[0]->cb_func) {
        array[0]->cb_func(array[0]->user_data);
      }
      pop();
      temp = array[0];
    }
  }

  bool empty() { return cur_size == 0; }

private:
  heap_timer **array; // 堆数组
  int cur_size;       // 当前元素个数
  int capcity;        // 容量

  void percolate_down(int hole) { // 往下调整堆
    heap_timer *temp = array[hole];
    int child = 0;
    for (; (hole * 2 + 1 <= cur_size - 1); hole = child) {
      child = hole * 2 + 1;
      if ((child < cur_size - 1) &&
          (array[child]->expire > array[child + 1]->expire)) {
        ++child;
      }
      if (temp->expire > array[child]->expire) {
        array[hole] = array[child];
      } else {
        break;
      }
    }
    array[hole]=temp;
  }

  void resize() noexcept(false) { // 扩容，capcity*2
    heap_timer **temp = new heap_timer *[capcity * 2];
    for (int i = 0; i < 2 * capcity; i++) {
      temp[i] = NULL;
    }
    if (!temp) {
      throw std::exception();
    }
    capcity = capcity * 2;
    for (int i = 0; i < cur_size; i++) {
      temp[i] = array[i];
    }
    delete[] array;
    array = temp;
  }
};

#endif