#include "EventLoop.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "EpollPoller.h"
#include <cstdint>
#include <mutex>
#include <sys//eventfd.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "Channel.h"
#include "Poller.h"
#include <memory>
#include <vector>

//防止一个线程创建多个eventloop
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的poller IO接口的超时时间
const int KPollTimeMs = 10000;

//创建wakefd,用来notify唤醒subReactor处理新来的channel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    if(evtfd < 0){
        LOG_FATAL("Eventfd error:%d", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    :looping_(false),
    quit_(false),
    callingPengingFunctors_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this,wakeupFd_))
    {
        LOG_DEBUG("EventLoop created %p in thread %d\n",this,threadId_);
        if(t_loopInThisThread){
            LOG_FATAL("another eventloop %p exists in this thread %d \n",t_loopInThisThread,threadId_);
        }else{
            t_loopInThisThread = this;
        }

        //设置wakeupFd的事件类型以及发生事件后的回调操作
        wakeupChannel_ -> setReadCallback(std::bind(&EventLoop::handleRead,this));
        //每一个enventloop都将监听wakeupchannel的EPOLLIN读事件
        wakeupChannel_ -> enableReading();
    }

EventLoop::~EventLoop(){
    wakeupChannel_ -> disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_,&one,sizeof one);
    if(n != sizeof one){
        LOG_ERROR("Eventloop::handleRead() erorr\n");
    }
}
//开启事件循环
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping",this);
    while(!quit_){
        activeChannels_.clear();
        //监听两种fd ，一种是wakeup fd, 一种是client的fd
        pollReturnTime_ = poller_->poll(KPollTimeMs, &activeChannels_);
        for(Channel* channel : activeChannels_){
            //poller监听哪些channel发生了事件，然后上报给eventLoop,通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前eventloop事件循环需要处理的回调操作
        /**
        IO 线程 mainloop -> accept -> fd 用channel打包fd,  main分发给subloop
        mainloop事先注册一个cb，需要subloop来执行    wakeup subloop来执行之前注册的回调
        */
        dopendingFunctors();  //执行回调
    }
    LOG_INFO("EventLoop %p stoping.\n",this);
}
//1.loop在自己的线程中调用quit， 
void EventLoop::quit(){
    quit_ = true;
    if(!isInLoopThread()){ //如果是在其他的线程中调用的quit，在一个subloop中调用的mainloop的quit
        wakeup();   //唤醒其他线程
    }
}

void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){//在当前的线程中执行callback
        cb();
    }else{//在非当前loop线程中执行callback
        queueInLoop(cb);
    }
}
void EventLoop::queueInLoop(Functor cb){

    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctions.emplace_back(cb);
    }
    //唤醒相应的，需要执行上面回调操作的loop线程
    if(!isInLoopThread()  || callingPengingFunctors_){ //当前loop正在执行回调，但是loop又有了新的回调
        wakeup();  //唤醒loop所在线程
    }
}
//用来唤醒所在线程,向wakeupfd_中写一个数据,wakeup就发生读事件，当前线程就会唤醒
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

void EventLoop::updateChannel(Channel* channel){
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}

void EventLoop::dopendingFunctors(){
    std::vector<Functor> functors;
    callingPengingFunctors_ = true;  //需要执行回调
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctions);
    }

    for(const auto functor:functors){
        functor();  //执行当前loop需要执行的回调操作
    }
    callingPengingFunctors_ = false;
}   