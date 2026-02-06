#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int numCreated_{0};  //静态成员变量需要在类外单独进行初始化
Thread::Thread(ThreadFunc func,const std::string& name)
    :started_(false),
    joined_(false),
    tid_(0),
    func_(std::move(func)),
    name_(name)
    {
        setDefaultName();
    }

Thread::~Thread(){
    if(started_ && !joined_){
        thread_->detach();  //thread设置了分离线程的方法
    }
}

void Thread::start(){ //一个Thread对象，记录的就是一个新线程的详细信息
    started_ = true;
    sem_t sem;
    sem_init(&sem,false,0);
    
    //开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_= CurrentThread::tid();  //获取当前的tid值
        sem_post(&sem);
        func_();//开启一个新线程，专门执行该线程函数
    }));

    sem_wait(&sem);
}
void Thread::join(){    
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName(){
    int num = ++numCreated_;
    if(name_.empty()){
        char buf[32] = {};
        snprintf(buf,sizeof buf,"Thread%d",num);
        name_ = buf;
    }
} 