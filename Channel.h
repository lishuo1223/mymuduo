#pragma once

#include <functional>
#include <memory>

#include "noncopyable.h"
#include "Timestamp.h"

//前置声明，减少头文件依赖
class EventLoop;

/** 
 * Channel理解为通道，封装了sockfd和其他感兴趣的event,如EPOLLIN,EPOLLOUT事件
 * 还绑定了poller返回的具体事件
*/
class Channel:noncopyable{
public:
    using EventCallback = std::function<void()>;   //空参数回调
    using ReadEventCallback = std::function<void(Timestamp)>;
    Channel(EventLoop* loop,int fd);
    ~Channel();
    //fd得到poller通知以后，处理事件，调用相应的回调方法
    void handleEvent(Timestamp receiveTime);
    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb){
        readCallback_ = std::move(cb);
    }

    void setWriteCallback(EventCallback cb){
        writeCallback_ = std::move(cb);
    }

    void setCloseCallback(EventCallback cb){
        closeCallback_ = std::move(cb);
    }

    void setErrorCloseCallback(EventCallback cb){
        errorCallback_ = std::move(cb);
    }
    //防止channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void>& );

    int fd() const{return fd_;}

    int events() const {return events_;}

    void set_revents(int revt) {revents_ = revt;}

    bool isNoneEvent() const {return events_ == KNoneEvent; }
    //设置fd相应的事件状态
    void enableReading() {events_ |= KReadEvent; update();}
    void disableReading() {events_ &= ~KReadEvent; update();}

    void enableWriting() {events_ |= KWriteEvent; update();} 
    void disableWriting() {events_ &= ~KWriteEvent; update();}

    void disableAll() {events_ |= KNoneEvent; update();}

    bool isWritingEvent() { return events_ & KWriteEvent;}
    bool isReadEvent() {return events_ & KReadEvent;}

    int index() {return index_;}
    void set_index(int idx) {index_ = idx;}

    EventLoop* ownerLoop() {return loop_;}
    void remove();
private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);
    static const int KNoneEvent;
    static const int KReadEvent;
    static const int KWriteEvent;
    EventLoop* loop_;            
    const int fd_;  //poller监听的对象
    int events_; //注册fd感兴趣的事件
    int revents_; //poller返回的具体发生的事件
    int index_; 

    std::weak_ptr<void> tie_;
    bool tied_;
    
    //因为channel通道里面能够感知fd最终发生的具体事件的revents,所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_; 
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};