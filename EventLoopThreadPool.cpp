#include <memory>
#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
        : baseLoop_(baseLoop)
        , name_(nameArg)
        , started_(false)
        , numThreads_(0)
        , next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;
    // 线程池开启的逻辑是：创建线程，并为线程绑定一个新的 EventLoop，然后分别将二者放到对应的队列
    for(int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));

        // 底层创建线程 绑定一个新的EventLoop 并返回该loop的地址
        loops_.push_back(t->startLoop());
    }

    if(numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}


EventLoop *EventLoopThreadPool::getNextLoop()
{
    // 初始指向 mainLoop
    EventLoop *loop = baseLoop_;
    // 轮询获取一个 subLoop
    if(!loops_.empty())
    {
        loop = loops_[next_];
        ++next_;
        if(next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if(loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}