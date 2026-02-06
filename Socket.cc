#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <netinet/tcp.h>
Socket::~Socket(){
    ::close(sockfd_);
}
void Socket::bindAddress(const InetAddress& localAddr){
    if(::bind(sockfd_,(sockaddr*)localAddr.getSockAddr(),sizeof(sockaddr_in)) != 0){
        LOG_FATAL("bind sockfd:%d fail\n",sockfd_);
    }
}
void Socket::listen(){
    if(::listen(sockfd_,1024) != 0){
        LOG_FATAL("listen fd:%d fail\n",sockfd_);
    }
}
int Socket::accept(InetAddress* peerAddr){
    sockaddr_in addr;
    bzero(&addr,sizeof addr);    
    socklen_t len;
    int connfd = ::accept(sockfd_,(sockaddr*)&addr,&len);
    if(connfd >= 0){
        peerAddr->setSockaddr(addr);
    }
    return connfd;
}
void Socket::shutdownWrite(){
    if(::shutdown(sockfd_,SHUT_WR)<0){
        LOG_ERROR("shutdownwrite error\n");
    }
}   
void Socket::setTcpNoDelay(bool on){
    int optval = on ? 1:0;
    ::setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof optval);
}
void Socket::setReuseAddr(bool on){
    int optval = on ? 1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof optval);
}
void Socket::setReusePort(bool on){
    int optval = on ? 1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof optval);
}
void Socket::setKeepAlive(bool on){
    int optval = on ? 1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof optval);
}
