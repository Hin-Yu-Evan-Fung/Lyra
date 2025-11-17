#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <vector>

namespace Lyra {

struct Thread {
public:
    Thread() : stop_(false), running_(false), thread_(&Thread::loop, this) {}
    Thread(Thread&& th) : stop_(th.stop_.load()), running_(th.running_.load()), thread_(std::move(th.thread_)) {}
    ~Thread() { stop(); }

    void wait();
    void stop();
    void exec(std::function<void()> func);

private:
    void loop();

    std::condition_variable cv_;
    std::mutex              mtx_;
    std::atomic_bool        stop_;
    std::atomic_bool        running_;
    std::thread             thread_;
    std::function<void()>   func_;
};

struct ThreadPool {
public:
    ThreadPool(size_t num) : threads_(num) {}
    ~ThreadPool() { stop(); }

    void resize(size_t num);
    void exec_all(std::function<void()> func);
    void stop();
    void wait();

private:
    std::vector<Thread> threads_;
    std::atomic_bool    stop_;
};

}  // namespace Lyra
