#include <sys/epoll.h>


#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

const int Channel::KNoneEvent = 0;
const int Channel::KReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::KWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop,int fd):
    loop_(loop),fd_(fd),events_(0),revents_(0),
    index_(-1),tied_(false){}
Channel::~Channel(){

}
void Channel::tie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}

// 当改变channel所表示fd的events事件后，update负责在poller里面更改fd对应的epoll_ctl
void Channel::update(){
    //通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    
    loop_->updateChannel(this);

}
//在channel所属的EventLoop中，将channel删除掉
void Channel::remove(){
    
    loop_->removeChannel(this);
}
void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }

    }
    else{
        handleEventWithGuard(receiveTime);
    }
}
void Channel::handleEventWithGuard(Timestamp receiveTime){
    LOG_INFO("channel handleEvent revents:%d\n",revents_);
    if((revents_ & EPOLLHUP) && (revents_ & EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }
    if(revents_ & EPOLLERR){
        if(errorCallback_){
        errorCallback_();
        }
    }

    if(revents_ & EPOLLIN){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }
    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}   



