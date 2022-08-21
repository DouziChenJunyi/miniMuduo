#pragma once
#include <vector>
#include <sys/epoll.h>
#include "Poller.h"
#include "Timestamp.h"

class Channel;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新 channel 通道 其实就是调用epoll_ctl
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;      // epoll_create创建返回的fd保存在epollfd_中
    EventList events_; // 用于存放epoll_wait返回的所有发生的事件的文件描述符事件集
};