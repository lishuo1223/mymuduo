#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <cstddef>
#include <functional>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

static EventLoop* CheckLoopNotNULL(EventLoop* loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null!\n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
                   const std::string& name,
                   int sockfd,
                   const InetAddress& localAddr,
                   const InetAddress& peerAddr
                    ):
                    loop_(CheckLoopNotNULL(loop)),
                    name_(name),
                    state_(KConnecting),
                    reading_(true),
                    socket_(new Socket(sockfd)),
                    channel_(new Channel(loop,sockfd)),
                    localAddr_(localAddr),
                    peerAddr_(peerAddr),
                    highWaterMark_(64*1024*1024)

                    {//给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生，channel会回调相应的操作函数
                        channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
                        channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
                        channel_->setCloseCallback(std::bind(&TcpConnection::handleClose,this));
                        channel_->setErrorCloseCallback(std::bind(&TcpConnection::handleError,this));
                        LOG_INFO("Tcp connection ctor[%s] at fd=%d\n",name_.c_str(),sockfd);
                        socket_->setKeepAlive(true);
                    }
TcpConnection::~TcpConnection(){
    LOG_INFO("TcpConncection::dtor[%s] at fd = %d state = %d\n",name_.c_str(),channel_->fd(),state_.load());
}

void TcpConnection::handleRead(Timestamp receiveTime){
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n > 0){
        //已建立连接的用户，有刻度事件发生了，调用用户传入的回调操作OnMessage
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
    }
    else if(n == 0){
        handleClose();
    }
    else{
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead!");
        handleError();
    }
}
void TcpConnection::handleWrite(){
    if(channel_->isWritingEvent()){
        int saveErrorno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(),&saveErrorno);
        if(n > 0){
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0){
                channel_->disableWriting();
                if(writeCompleteCallback_){
                    //唤醒loop对应的线程的回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_,shared_from_this())
                    );
                }
                if(state_ == KDisConnecting){
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection:: handleWrite");
        }
        
    }
    else{
            LOG_ERROR("TcpConnection fd=%d is down,no more writting\n",channel_->fd());
        }
}
void TcpConnection::handleClose(){
    LOG_INFO("fd=%d state=%d\n",channel_->fd(),state_.load());
    setState(KDisconnected);
    channel_->disableAll();
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);  //执行连接关闭的回调
    closeCallback_(connPtr); //关闭连接的回调

}
void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval,&optlen) < 0){
        err = errno;
    }
    else{
        err = optval;
    }
}
void TcpConnection::send(const std::string& buf){
    if(state_ == KConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buf.c_str(),buf.size());
            
        }
        else{
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,this,buf.c_str(),buf.size()
            ));
        }
    }
}

void TcpConnection::sendInLoop(const void* data, size_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    if(state_ == KDisconnected){
        LOG_ERROR("disconnected give up writing!");
        return;
    }
    if(!channel_->isWritingEvent() && outputBuffer_.readableBytes() == 0){  //channel第一次开始写数据，而且缓冲区没有待发送数据
        nwrote = ::write(channel_->fd(), data, len);

        if(nwrote >= 0){
            remaining = len -nwrote;
            if(remaining == 0 && writeCompleteCallback_){
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_,shared_from_this())                );
            }
        }
        else{
            nwrote = 0;
            if(errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::sendInLoop!");
                if(errno == EPIPE || errno == ECONNRESET){
                    faultError = true;
                }
            }
        }
    }
    //当前这一次write，没有把数据全部发出去，剩余的数据需要存到缓冲区中，然后给channel
    if(!faultError && remaining > 0){
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_&& highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldLen+remaining));
        }
        outputBuffer_.append((char*)data+nwrote,remaining);
        if(!channel_->isWritingEvent()){
            channel_->enableWriting();
        }
    }
}
void TcpConnection::connectionEstablished(){//连接建立
    setState(KConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    //新连接建立
    connectionCallback_(shared_from_this());
}
void TcpConnection::connectionDestoryed(){ //连接销毁
    if(state_ == KConnected){
        setState(KDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}
void TcpConnection::shutdown(){
    if(state_==KConnected){
        setState(KDisConnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }
}
void TcpConnection::shutdownInLoop(){
    if(!channel_->isWritingEvent()){
        socket_->shutdownWrite(); //关闭写端
    }
}