#pragma once
#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include <atomic>
#include <unordered_map>
/**
 *用户使用muduo编写服务器程序
 */
#include <functional>
//对外的服务器编程使用的类
class TcpServer : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    
    enum Option{
        KNoReusePort,
        KReusePort,
    };
    TcpServer(EventLoop* loop,
    const InetAddress& listenAddr,
    const std::string& nameArg,
    Option option = KNoReusePort);
    ~TcpServer();
    void setThreadInitCallback(const ThreadInitCallback &cb) {
        threadInitCallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback &cb){
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback &cb){
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){
        writeCompleteCallback_ = cb;
    }
    void setThreadNum(int numThreads); //设置底层subloop个数

    void start();  //开启服务器监听
private:
    void newConnection(int sockfd,const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr&);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
    using ConnectionMap = std::unordered_map<std::string,TcpConnectionPtr>;
    EventLoop* loop_;
    const std::string port_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;  //有新连接时的回调
    MessageCallback messageCallback_;   //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  //消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_;   //loop线程初始化回调

    std::atomic_int started_;
    int nextConnId_;
    ConnectionMap connections_;  //保存所有的连接
};