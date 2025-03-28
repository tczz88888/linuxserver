#ifndef THREADPOOL_H
#define THREADPOOL_H
#include "list"
#include "cstdio"
#include "exception"
#include "pthread.h"

#include "locker.h"
#include <cstdio>
#include <exception>
#include <list>

template<typename T>
class threadpool{
    public:
        threadpool(int thread_number=8,int max_request=10000);
        ~threadpool();
        /*向队列添加任务*/
        bool append(T* request);
    
    private:
        int m_thread_number;//线程池中线程数量
        int m_max_request;//请求队列最大容量
        pthread_t * m_threads;//存线程号
        std::list<T*>m_workqueue;//请求队列
        locker m_queuelocker;//保护请求队列的互斥锁
        sem m_queuestat;//是否有任务需要处理
        bool m_stop;//是否结束线程
        /*工作线程的运行函数，不断从任务队列里面拿任务出来做*/
        static void *worker(void *arg);
        void run();
};

template<typename T>//创建并初始化线程池
threadpool<T>::threadpool(int thread_number,int max_request):m_thread_number(thread_number),m_max_request(max_request),m_threads(nullptr),m_stop(false){
    if(thread_number<=0||m_max_request<=0){
        throw std::exception();
    }
    m_threads=new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    for(int i=0;i<m_thread_number;i++){
        printf("create the %dth thread \n",i);
        if(pthread_create(m_threads+i, nullptr, worker, this)!=0){
            delete [] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>//创建并初始化线程池
threadpool<T>::~threadpool(){
    delete [] m_threads;
    m_stop=true;
}

template<typename  T>//操作工作队列
bool threadpool<T>::append(T* request){
    /*一定要枷锁，被所有线程共享*/
    m_queuelocker.lock();
    if(m_workqueue.size()>m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void * threadpool<T>::worker(void *arg){
    threadpool *pool=(threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request=m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;;
        }
        request->process();
    }
}

#endif