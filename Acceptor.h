#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include <functional>
#include "InetAddress.h"

class Acceptor:noncopyable{
public:
    using NewConnectionCallback = std::function<void(int sockfd,const InetAddress&)>;
    explicit Acceptor(EventLoop* loop,const InetAddress& listenAddr,bool reuseport);
    ~Acceptor();
    void setNewConnectionCallback(const NewConnectionCallback &cb){
        newConnectionCallback_ = cb;
    }
    bool listenning() const{
        return listenning_;
    }
    void listen();
private:
    void handleRead();
    EventLoop* loop_;  //acceptor用的就是用户定义的那个baseloop,也称作mainloop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};