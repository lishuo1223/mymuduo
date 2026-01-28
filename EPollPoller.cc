#include <asm-generic/errno-base.h>
#include <bits/types/error_t.h>
#include <cerrno>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>

#include "Logger.h"
#include "EpollPoller.h"
#include "Channel.h"
#include "Timestamp.h"
const int KNew = -1; //channle index_ 初始化就是-1，表示channel未添加到poller中
const int KAdded = 1; //channel 已添加到poller中
const int KDeleted = 2;  //channel从poller中删除

EPollPoller::EPollPoller(EventLoop* loop):
    Poller(loop),epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    events_(KInitEventListSize){
        if(epollfd_ < 0){
            LOG_FATAL("epoll create error:%d\n",errno);
        }
}
EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}
//epoll_wait
Timestamp EPollPoller::poll(int timeouts,ChannelList *activeChannels) {
    //实际上用DEBUG更合理
    LOG_INFO("func=%s fd total count:%ld\n",__FUNCTION__,channels_.size());    
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeouts);
    int saveErrno = errno;
    Timestamp now(Timestamp::Now());
    if(numEvents > 0){
        LOG_INFO("%d events happened\n",numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size()){
            events_.resize(events_.size()*2);
        }
    }else if(numEvents == 0){
        LOG_DEBUG("%d timeout!\n",__FUNCTION__);
    }else{
        if(saveErrno!= EINTR){
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

//channel update remove => EventLoop updatechannel removechannel => poller updatechannel removechannel
/**
 *          EventLoop => poller.poll
 * ChannelList   Poller
 *            ChannelMap<fd,channel*>
 */
void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("func=%s fd=%d events=%d index=%d",__FUNCTION__,channel->fd(),channel->events(),index);

    if(index == KNew || index == KDeleted){
        if(index == KNew){
            int fd = channel -> fd();
            channels_[fd] = channel;
        }
        channel ->set_index(KAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else{ //channel 已经在poller上注册过了
        int fd = channel->fd();
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(KDeleted);
        }
        else{
            update(EPOLL_CTL_MOD,channel);
        }
    }
}
    
//从poller中删除channel
void EPollPoller::removeChannel(Channel* channel) {
    int fd = channel->fd();
    int index = channel->index();
    LOG_INFO("func=%s fd=%d",__FUNCTION__,fd);
    channels_.erase(fd);
    if(index == KAdded){
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(KNew);
}

void EPollPoller::fillActiveChannels(int numEvents,ChannelList* activeChannels) const{
    for(int i = 0; i < numEvents; i ++){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

//更新channel通道， epoll_ctl add/mod/del
void EPollPoller::update(int operation,Channel* channel){
    epoll_event event;
    memset(&event,0,sizeof event);
    event.events = channel->events();
    event.data.fd = channel->fd();
    event.data.ptr = channel;

    int fd = channel->fd();
    if(::epoll_ctl(epollfd_,operation,fd,&event) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error %d\n",errno);
        }
        else{
            LOG_FATAL("epoll_ctl add/mod %d\n",errno);
        }
    }
}