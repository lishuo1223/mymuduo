#include "EventLoopThread.h"
#include "EventLoop.h"
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name ):
    loop_(nullptr),
    exiting_(false),
    thread_([this](){
        threadFunc();
    },name),
    mutex_(),
    cond_(),
    callback_(cb)
    {

    }
    
EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if(loop_ != nullptr){
        loop_ -> quit();
        thread_.join();
    }
}
EventLoop* EventLoopThread::startLoop(){
    thread_.start();  //启动底层线程
    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop == nullptr){
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}
//下面这个方法是在单独的新线程里面运行的 
void EventLoopThread::threadFunc(){
    EventLoop loop;   //创建一个独立的eventloop
    if(callback_){
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop(); //执行了Eventloop的poller.poll函数，
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}