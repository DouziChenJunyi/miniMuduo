项目介绍：本项目为个人项目，旨在实践 muduo 网络库的学习。
个人职责：后端开发
工作内容：核心类包括事件循环类 EventLoop + 对 socketfd 进行封装的通道类 Channel + 事件分发器类 Poller + IO 线程池类 EventLoopThreadPool + 缓冲区类 Buffer 对新连接 accept 进行封装的类 Acceptor + 管理一个 TCP 连接的类 TcpConnection + 管理所有 TCP 连接的服务器类 TcpServer。基于 miniMuduo 网路库编写一个回射服务器验证实例。
