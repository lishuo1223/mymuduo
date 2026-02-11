#include "TcpServer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include <cstddef>
#include <cstdio>
#include <functional>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include "TcpConnection.h"

static EventLoop* CheckLoopNotNULL(EventLoop* loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d mainLoop is null!\n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
    const InetAddress& listenAddr,
    const std::string& nameArg,
    Option option):
    loop_(CheckLoopNotNULL(loop)),
    port_(listenAddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop,listenAddr,option == KReusePort)),
    threadPool_(new EventLoopThreadPool(loop_,name_)),
    connectionCallback_(),
    messageCallback_(),
    nextConnId_(1)    
    {
        acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,
        std::placeholders::_1,std::placeholders::_2));


    }
void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr){
    EventLoop* ioLoop = threadPool_->getNetLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d",port_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConncetion [%s] - new connection [%s] from %s \n",name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());
    
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0){
        LOG_ERROR("socket::getLoacalAddr");
    }
    InetAddress localAddr(local);

    //根据连接成功的sockfd,创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,connName,sockfd,localAddr,peerAddr)
    );
    connections_[connName] = conn;
    //回调用户设置给Tcpserver->TcpConnection -> channel -> poller -> notify channel 调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection,this,std::placeholders::_1)
    );

    ioLoop->runInLoop(std::bind(&TcpConnection::connectionEstablished,conn));

}
void TcpServer::removeConnection(const TcpConnectionPtr& conn){
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop,this,conn)
    );
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection  %s\n",name_.c_str(),conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectionDestoryed,conn));
}
void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}
void TcpServer::start(){
    if(started_++ == 0){ //防止一个TcpServer对象被重复启动多次
        threadPool_->start(threadInitCallback_); //启动底层loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
    }
}


TcpServer::~TcpServer(){
    for(auto &item:connections_){
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectionDestoryed,conn)
        );
    }
}

