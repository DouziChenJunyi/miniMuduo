#pragma once // 防止头文件重复包含

// 继承该基类的派生类的拷贝构造和赋值构造接口被封住了
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};