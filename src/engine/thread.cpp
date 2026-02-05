#include "engine/thread.hpp"

#include <functional>
#include <mutex>

namespace Lyra {

/******************************************\
|==========================================|
|                  Thread                  |
|==========================================|
\******************************************/

Thread::Thread(std::atomic_bool &stop, size_t id, TT &tt)
    : id_(id), worker_(stop, id, tt), exit_(false), busy_(false),
      thread_(&Thread::loop, this) {}

Thread::~Thread() {
  exit_ = true;
  cv_.notify_one(); // Notify the idle loop that the loop must exit as soon as
                    // possible
  if (thread_.joinable())
    thread_.join();
}

void Thread::wait() {
  std::unique_lock<std::mutex> lock(mtx_);
  cv_.wait(lock, [&] { return !busy_; });
}

void Thread::exec(std::function<void(Thread &)> func) {
  {
    std::unique_lock<std::mutex> lock(mtx_);
    func_ = std::move(func);
  }
  cv_.notify_one();
}

void Thread::loop() {
  while (!exit_) {
    std::function<void(Thread &)> func;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.notify_one(); // Notify any wait calls that the function is finished
      cv_.wait(lock, [this] {
        return func_ != nullptr || exit_;
      }); // Wait for new job or exit command
      if (exit_)
        return;
      func = std::move(func_);
      func_ = nullptr; // Reset the loop
    }

    if (func) {
      busy_ = true;
      func(*this);
      busy_ = false;
    }
  }
}

/******************************************\
|==========================================|
|               Thread Pool                |
|==========================================|
\******************************************/

ThreadPool::ThreadPool(size_t num, TT &tt) : stop_(false) { resize(num, tt); }

void ThreadPool::resize(size_t num, TT &tt) {
  wait();

  if (threads_.size() > 0)
    threads_.clear();

  threads_.reserve(num);

  while (threads_.size() < num)
    threads_.emplace_back(std::make_unique<Thread>(stop_, threads_.size(), tt));

  wait();
}

void ThreadPool::exec(std::function<void(Thread &)> func) {
  for (auto &thread : threads_)
    thread->exec(func);
}

void ThreadPool::wait() {
  for (auto &thread : threads_)
    thread->wait();
}

bool ThreadPool::is_busy() {
  for (auto &thread : threads_)
    if (thread->is_busy())
      return true;
  return false;
}

void ThreadPool::stop() {
  stop_ = true;
  wait();
  stop_ = false;
}

} // namespace Lyra
