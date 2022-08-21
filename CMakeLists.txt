cmake_minimum_required(VERSION 3.19)
project(miniMuduo)

set(CMAKE_CXX_STANDARD 11)

add_executable(miniMuduo main.cpp Acceptor.h noncopyable.h Acceptor.cpp Channel.h Channel.cpp Buffer.h Buffer.cpp EventLoop.h EventLoop.cpp EventLoopThread.h EventLoopThread.cpp EventLoopThreadPool.h EventLoopThreadPool.cpp InetAddress.h InetAddress.cpp Poller.h Poller.cpp Thread.h Thread.cpp TimeStamp.h TimeStamp.cpp TcpConnection.h TcpConnection.cpp TcpServer.h TcpServer.cpp Socket.h Socket.cpp Logger.h Logger.cpp DefaultPoller.cpp echoServer.cpp EPollPoller.h)