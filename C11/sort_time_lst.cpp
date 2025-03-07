#ifndef LST_TIMER
#define LST_TIMER

#include <ctime>
#include <time.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define BUFFER_SIZE 64

class util_timer;


class client_data{//如何才能向一个链接发送数据呢，肯定要一个sockfd一个buf，然后用util_timer做计时器，并且存一下对应的sockaddr
    public:
    sockaddr_in client_addr;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer *timer;
};

class util_timer{//计时器，需要一个超时时间，指向前驱和后继的指针，一个callback用于到期后的处理，需要一个client才知道通知谁到期了
    public:
        time_t expire;
        util_timer * pre;
        util_timer * nxt;
        client_data *user_data;
        void (*cb_func)(client_data *);
};

class sort_time_lst{
    public:
        sort_time_lst():head{nullptr},tail{nullptr}{}
        ~sort_time_lst(){
            util_timer *tmp=head;
            while(tmp){
                head=tmp->nxt;
                delete tmp;
                tmp=head;
            }
        }

        void add_timer(util_timer *timer){
            if(!timer) return ;
            if(!head){
                head=tail=timer;
                return ;
            }
            if(timer->expire<head->expire){
                timer->nxt=head;
                head->pre=timer;
                head=timer;
                return ;
            }
            add_timer(timer,head);
        }

        void delete_timer(util_timer *timer){
            if(!timer) return ;
            if((timer==head)&&(timer==tail)){
                delete timer;
                head=tail=nullptr;
                return ;
            }

            if(timer==head){
                head=head->nxt;
                head->pre=nullptr;
                delete timer;
                return ;
            }

            if(timer==tail){
                tail->pre->nxt=nullptr;
                tail=tail->pre;
                delete timer;
                return ;
            }

            timer->pre->nxt=timer->nxt;
            timer->nxt->pre=timer->pre;
            delete timer;
        }

        void tick(){
            if(!head) return ;
            printf("timer tick\n");
            time_t cur=time(NULL);
            util_timer *tmp=head;
            while(tmp){
                if(cur<tmp->expire){
                    break;
                }
                tmp->cb_func(tmp->user_data);
                head=tmp->nxt;
                if(head) head->pre=nullptr;
                delete tmp;
                tmp=head;
            }
        }


    private:
        void add_timer(util_timer *timer,util_timer* lst_head){
            util_timer *pre=lst_head;
            util_timer *tmp=pre->nxt;
            while(tmp){
                if(timer->expire<tmp->expire){
                    pre->nxt=timer;
                    timer->nxt=tmp;
                    tmp->pre=timer;
                    timer->pre=pre;
                    break;
                }
                pre=tmp;
                tmp=tmp->nxt;
            }

            if(!tmp){
                pre->nxt=timer;
                timer->pre=tail;
                timer->nxt=nullptr;
                tail=timer;
            }
        }
        util_timer* head;
        util_timer* tail;
};

#endif