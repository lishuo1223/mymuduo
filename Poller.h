#pragma once

#include <vector>
#include <unordered_map>

#include "noncopyable.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
/**
 * 
 * muduo库中多路事件分发器的核心，IO复用模块
 * 
 * 
 */
class Poller:noncopyable{
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop* loop);
    virtual ~Poller() = default; 
    //保留统一的给所有IO复用的接口
    virtual Timestamp poll(int timeoutMs,ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
    //判断参数channel是否在当前poller当中
    bool hasChannel(Channel* channel) const;
    //事件循环可以通过改接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);
private:
    EventLoop* ownerLoop_;  //定义poller所属的事件循环Eventloop
protected:
    //map的key：sockfd   value:sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;
};