项目介绍：本项目为个人项目，旨在实践 muduo 网络库的学习。
个人职责：后端开发
工作内容：核心类包括事件循环类 EventLoop + 对 socketfd 进行封装的通道类 Channel + 事件分发器类 Poller + IO 线程池类 EventLoopThreadPool + 缓冲区类 Buffer 对新连接 accept 进行封装的类 Acceptor + 管理一个 TCP 连接的类 TcpConnection + 管理所有 TCP 连接的服务器类 TcpServer。基于 miniMuduo 网路库编写一个回射服务器验证实例。

## 【muduo 总结——篇 1】对 muduo 六大核心类的关系梳理

![](https://img2022.cnblogs.com/blog/1466728/202208/1466728-20220830161515825-1281282758.png)

备注：Reactor 与 EventLoop 可互换，Reactor 是一种模式，EventLoop 是具体类的名称。
### 看左边——把握 TcpServer、Acceptor、TcpConnection 三类关系
TcpServer 类包含唯一一个 Main Reactor（EventLoop 类），类包含唯一一个 Acceptor 类。Acceptor 类包含封装了 listenfd 的 Channel 类。TcpServer 事先给 listenfd 进行 bind 及 listen 操作进行端口监听。当有新连接到来时， Acceptor 会调用 newConnection() 创建函数，创建 TcpConnection 对象，并给该对象设置读写回调。TcpConnection 对象包含唯一一个 Channel，Main Reactor 向 EventLoop 注册 Channel 的读写事件。此外，TcpConnection 还包含输入缓冲区 Read Buffer 、输出缓冲区 Write Buffer。

### 看右边——把握 Channel、EventLoop（Sub Reactor）、Poller 三类关系
Channel 不能直接与 Poller交互，必须通过 EventLoop。EventLoop 挂载多个 Channel，由 EventLoop 对应的内部类 Poller() 监听这些 Channel 的事件。若 Poller 监听到某 Channel 有事件发生，将活动事件放到 EventLoop 的 activeChannels 中，由 EventLoop 依次通知 Channel 执行自身的回调。
**注意：** Channel 是封装了 fd 及执行 fd 事件处理函数的类。

### 看右边——把握 Thread 与 EventLoop 的关系
Thread 与 EventLoop 是一一对应的关系，且 EventLoop 的读写事件只能由对应的线程处理，目的是避免资源争夺。 Thread 创建逻辑是创建一个 EventLoop，对于被创建的 EventLoop ，创建它的 Thread 称为 IO Thread，其他 Thread 称为非 IO Thread。当 IO 线程调用 EventLoop 的回调函数时，回调会同步进行；若是非 IO Thread 调用 EventLoop 的回调函数，非 IO Thread 会将该回调放入 EventLoop 的回调函数任务队列，然后唤醒 IO Thread 执行此回调。

### 看右边——把握线程池 ThreadPool 
ThreadPool 一运行，就创建 Num 个 Thread，Thread 创建出对应的 EventLoop，二者分别放到 ThreadPool 的 Thread 队列和 EventLoop 队列。

### 看中间——注册 Channel 的读写事件
【看左边——把握 TcpServer、Acceptor、TcpConnection 三类关系】谈到 Main Reactor 向 EventLoop 注册 Channel 的读写事件。
具体言之：Main Reactor 从线程池中取出一个 EventLoop，将 Channel 挂载在其中一个 EventLoop，这个过程是需要 Main Reactor 通过 wakeup() 函数唤醒 Sub Reactor。
之所以要唤醒 Sub Reactor，是因为 Sub Reactor 可能阻塞在 `epoll_wait()` 处。
【精妙之处】Main Reactor 通过写 1 字节数据唤醒 Sub Reactor，然后它才能执行 doFunctors() 为 Channel 进行注册读写事件。
【考点】notify_one() 与 notify_all() 的区别：
otify_one() 与 notify_all() 用来唤醒阻塞的线程
· **notify_one()** ：只唤醒等待队列中的第一个线程，不存在锁竞争，所以能够立即获得锁。其余的线程不会被唤醒，需要等待再次调用 `notify_one()` 或 `notify_all()`。
· **notify_all()**：唤醒所有等待队列中阻塞的线程，存在锁竞争，只有一个线程能够获得锁。其它未获得锁的线程会继续尝试获得锁，但不会再次阻塞。当持有锁的线程释放锁时，这些线程中的一个会接着尝试获得锁。

### 思考之一
One Loop Per Thread + ThreadPool 将 connfd 分摊到多个 Sub Reactor，减少了单个 Sub Reactor 挂载的 fd 个数，减少了单个 Sub Reactor 的同步等待时间。

### 把握 TcpConnection::Buffer 类的设计思想
#### 为什么非阻塞网络编程中应用层 buffer 是必须的？
非阻塞 IO 的核心思想是避免阻塞在 read() 或 write() 或其他 IO 系统调用上，这样才能最大限度地让一个线程能服务于多个 socket 连接。IO 线程只能阻塞在 epoll_wait() 上。基于这个思想，应用层的缓冲是必须的，每个 TCP socket 都要有输入缓冲区、输出缓冲区。

#### TcpConnection 必须要有输出缓冲区？
考虑如下场景：若程序想通过 TCP 连接发送 100k 字节的数据，但是在 writ() 调用中，OS 只接收了 80k 字节（受滑动窗口的影响）。由于程序不能阻塞，要尽快交出控制权。因此，如果没有缓冲区 20k 数据将丢失。
对于应用程序而言，它只负责生成数据，只需调用 TcpConnection::send() 就认为数据迟早会发送出去。数据的真正分发工作由网络库负责。
· 网络库应该使用输出缓冲区接收剩余的 20k 字节数据，然后注册 POLLOUT 事件。若有 POLLOUT 事件发生，说明发送 socket 缓冲区为空，可以再剩余的数据再次发送。
· 发送完 20k 字节后，网络库停止关注 POLLOUT，以免造成 busy loop（关注的事件一直触发称为busy loop）。
· 若 20k 字节未发送前，又有新的 50k 字节需要 send()，网络库会先将这 50k 字节添加在 20k 字节之后，等 socket 可写的时候一并写入。
· 若输入缓冲区还有待发送的数据，而应用程序向关闭，网络库不能立刻关闭连接，而要等数据发送完毕。
【备注：muduo 把"主动关闭连接"这件事情分成两步走，首先关闭本地”写“端(shutdown 操作)，等对方关闭之后，再关本地”读“端。因为要防止在本地关闭连接的时候，对方发送了消息而本地未收到】

#### TcpConnection 必须要有输入缓冲区
由于 TCP 是无边界的字节流协议，所以服务端单次接收到的数据可能会出现数据不完整情况。因此网络库在处理 ”socket 可读“事件的时候，为了避免反复触发 POLLIN 事件，造成 busy-loop 必须一次性把 socket 的数据读完（从内核 buffer 读到应用层 buffer）。

#### Buffer 的设计要点：
· 首先是一块连续的内存；
· 其次，其 size() 可以自动增长，以适应不同大小的情况；

#### muduo 的 Buffer 是线程安全的吗？
不是线程安全的。
· 对于输入 buffer，只有 TcpConnection 所属的 IO 线程才能操作输入buffer，因此它不必是线程安全的。
· 对于输出 buffer，它不会暴露给用户程序，而是通过调用 TcpConnection::send() 来发送数据，而 send() 是安全的。解释如下：send() 只会发生在该 TcpConnection 所属的 IO 线程。如果调用 send() 是当前 IO 线程，那么它会通过调用 TcpConnection::sendInLoop() 操作输出 buffer。如果调用 send() 是其他 IO 线程，那么它会通过 runInLoop() 把 sendInLoop() 调用转移到所属 IO 线程。

#### muduo Buffer 的数据结构
 readIndex 和 writeIndex 将缓冲区分成 3 个部分 —— prependable + readable + writable
1）readIndex 和 writeIndex 在挪动过程中再次相等时，会重新归位。在 prependable = 8B 的位置。
2）Buffer 具有自动增长的功能；
3）内部腾挪。若经过若干次读写，readIndex 移到了比较靠后的位置，留下了巨大的 prependable 的空间，buffer 会将已有的数据移到前面去。
