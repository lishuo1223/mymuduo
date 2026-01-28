#pragma once
#include <vector>
#include <sys/epoll.h>

#include "Poller.h"
class Channel;

/**
 epoll的使用
 epoll_create
 epoll_ctl add/mod/del
 epoll_wait
 */
class EPollPoller: public Poller{
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;
    Timestamp poll(int timeouts,ChannelList *activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
private:
    static const int KInitEventListSize = 16;
    //填写活跃的连接
    void fillActiveChannels(int numEvents,ChannelList* activeChannels) const;
    //更新channel通道
    void update(int operation,Channel* channel);
    using EventList = std::vector<epoll_event>;
    int epollfd_;
    EventList events_; 
};