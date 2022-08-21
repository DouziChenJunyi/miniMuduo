#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include "EventLoop.h"
#include "Logger.h"
#include "Channel.h"
#include "Poller.h"

// EventLoop 与 IO 线程，二者是一一对应的关系
// 使用线程局部存储 __thread，保证每个线程有独立的 EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// Poll 超时时间
const int kPollTimeMs = 10000;


// 创建wakeupfd 用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
        : looping_(false)
        , quit_(false)
        , callingPendingFunctors_(false)
        , threadId_(CurrentThread::tid())
        , poller_(Poller::newDefaultPoller(this))
        , wakeupFd_(createEventfd())
        , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    // 防止一个线程创建多个 EventLoop
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(
            std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel_的EPOLL读事件了
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    // 移除 Channel注册的所有感兴趣事件
    wakeupChannel_->disableAll();
    // 移除 Channel 在 EventLoop上的映射关系
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        pollRetureTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生了事件 然后上报给EventLoop 通知channel处理相应的事件
            channel->handleEvent(pollRetureTime_);
        }
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping.\n", this);
    looping_ = false;
}


void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread())
    {
        wakeup();
    }
}

// 若 loop 在对应的 IO 线程调用，则直接调用回调函数；
// 否则，将回调函数放入 queueInLoop，并唤醒 EventLoop 所在线程执行 cb
void EventLoop::runInLoop(Functor cb)
{
    // 若是当前 IO 线程调用 runInLoop，则同步调用 cb
    if (isInLoopThread()) // 当前EventLoop中执行回调
    {
        cb();
    }
    else // 在非当前EventLoop线程中执行cb，就需要唤醒EventLoop所在线程执行cb
    {
        queueInLoop(cb);
    }
}

// 把cb放入队列中 唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
    }
}

// 用来唤醒 loop 所在线程 向wakeupFd_写一个数据 wakeupChannel就发生读事件 当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 特色：交换的方式减少了锁的临界区范围， 提升效率 同时避免了死锁
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor();
    }

    callingPendingFunctors_ = false;
}
