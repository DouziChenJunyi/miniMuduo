#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    Timestamp pollReturnTime() const { pollRetureTime_; }

    // 在当前loop中执行
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    // 通过eventfd唤醒loop所在的线程
    void wakeup();

    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断EventLoop对象是否在自己的线程里
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); } // threadId_为EventLoop创建时的线程id CurrentThread::tid()为当前线程id

private:
    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;

    // one thread per loop 每个 EventLoop 只能有一个线程
    const pid_t threadId_;

    Timestamp pollRetureTime_;
    std::unique_ptr<Poller> poller_;

    // 通过向 wakeupfd_ 写 8字节数据，来达到唤醒该 EventLoop，并让其处理 channel
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;
    // 相当于 ready list
    ChannelList activeChannels_;
    // 标识当前loop是否有需要执行的回调操作
    std::atomic_bool callingPendingFunctors_;
    // 存储loop需要执行的所有回调操作
    std::vector<Functor> pendingFunctors_;
    // 使用互斥锁，保证 pendingFunctors_ 的线程安全性
    std::mutex mutex_;
};