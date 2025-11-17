#include "thread.hpp"

#include <functional>
#include <memory>
#include <mutex>

namespace Lyra {

/******************************************\
|==========================================|
|                  Thread                  |
|==========================================|
\******************************************/

void Thread::wait() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [&] { return !running_; });
}

void Thread::stop() {
    stop_ = true;
    cv_.notify_all();  // Stop threads with stop == true;
    if (thread_.joinable()) thread_.join();
}

void Thread::exec(std::function<void()> func) {
    {
        std::unique_lock<std::mutex> lock(mtx_);
        func_ = std::move(func);
    }
    cv_.notify_one();
}

void Thread::loop() {
    while (!stop_) {
        std::function<void()> func;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return func_ != nullptr || stop_; });
            if (stop_) return;
            func  = std::move(func_);
            func_ = nullptr;
        }

        if (func) {
            running_ = true;
            func();
            running_ = false;
        }
    }
}

/******************************************\
|==========================================|
|               Thread Pool                |
|==========================================|
\******************************************/

void ThreadPool::resize(size_t num) {
    wait();

    threads_.clear();
    threads_.reserve(num);

    while (threads_.size() < num)
        threads_.emplace_back();

    wait();
}

void ThreadPool::exec_all(std::function<void()> func) {
    for (Thread& thread : threads_)
        thread.exec(func);
}

void ThreadPool::stop() {
    stop_ = true;
    for (Thread& thread : threads_)
        thread.stop();
}

void ThreadPool::wait() {
    for (Thread& thread : threads_)
        thread.wait();
}

}  // namespace Lyra
