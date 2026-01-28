#include "EpollPoller.h"
#include "Poller.h"
#include <stdlib.h>

//避免基类依赖子类
Poller* Poller::newDefaultPoller(EventLoop *loop){
    if(::getenv("MUDUO_USE_POLL")){
        return nullptr;  //生成poll实例
    }
    else{
        return new EPollPoller(loop); //生成epoll实例
    }
}