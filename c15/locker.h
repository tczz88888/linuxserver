//同步机制包装类
#ifndef LOCKER_H
#define LOCKER_H
#include "exception"
#include "pthread.h"
#include "semaphore.h"
#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem{
    /*创建并初始化信号量*/
    private:
        sem_t m_sem;
    public:
        sem(){
            if(sem_init(&m_sem, 0, 0)!=0){
                /*构造函数没有返回值，可以通过抛出异常来判断错误*/
                throw std::exception();
            }
        }
        /*销毁信号量*/
        ~sem(){
            sem_destroy(&m_sem);
        }

        bool wait(){
            return sem_wait(&m_sem)==0;
        }

        bool post(){
            return sem_post(&m_sem)==0;
        }
};

class locker{
    private:
        pthread_mutex_t m_mutex;
    public:
    /*创建初始化互斥锁*/
    locker(){
        if(pthread_mutex_init(&m_mutex, nullptr)!=0){
            throw std::exception();
        }
    }
    /*销毁互斥锁*/
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }

    /*加锁*/
    bool lock(){
        return pthread_mutex_lock(&m_mutex)==0;
    }

    /*释放锁*/
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex)==0;
    }
};


class cond{
    private:
        pthread_mutex_t m_mutex;
        pthread_cond_t m_cond;

    public:
        cond(){
            if(pthread_mutex_init(&m_mutex, nullptr)!=0){
                throw std::exception();
            }
            if(pthread_cond_init(&m_cond, nullptr)!=0){
                pthread_mutex_destroy(&m_mutex);
                throw std::exception();
            }
        }

        bool wait(){
            int ret=0;
            pthread_mutex_lock(&m_mutex);
            ret=pthread_cond_wait(&m_cond, &m_mutex);
            pthread_mutex_unlock(&m_mutex);
            return ret;
        }

        bool signal(){
            return pthread_cond_signal(&m_cond);
        }

        ~cond(){
            pthread_mutex_destroy(&m_mutex);
            pthread_cond_destroy(&m_cond);
        }
};
#endif