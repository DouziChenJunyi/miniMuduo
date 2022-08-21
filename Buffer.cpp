#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include "Buffer.h"


ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    // buffer_ 的 64 KB 栈缓存空间，用于在 buffer 扩充期间，暂存数据
    char extrabuf[65536] = {0};

    // 使用iovec分配两个连续的缓冲区
    struct iovec vec[2];
    // 剩余可写空间大小
    const size_t writable = writableBytes();

    // 第一块缓冲区，指向可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // 第二块缓冲区，指向栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // 只有在 buffer 缓冲区不够用的时候，才使用栈缓存空间
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable)
    {
        writerIndex_ += n;
    }
    else
    {
        writerIndex_ = buffer_.size();
        // 对buffer_扩容 并将extrabuf存储的另一部分数据追加至buffer_
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}