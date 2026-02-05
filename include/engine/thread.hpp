#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <vector>

#include "search/search.hpp"

namespace Lyra {

class Thread {
public:
  Thread(std::atomic_bool &stop, size_t id, TT &tt);
  ~Thread();

  void wait();
  void exec(std::function<void(Thread &)> func);

  bool is_main() { return id_ == 0; }
  bool is_busy() { return busy_; }

  size_t id_;
  Worker worker_;

private:
  void loop();

  std::condition_variable cv_;
  std::mutex mtx_;
  std::atomic_bool exit_;
  std::atomic_bool busy_;
  std::thread thread_;
  std::function<void(Thread &)> func_;
};

class ThreadPool {
public:
  ThreadPool(size_t num, TT &tt);
  ~ThreadPool() { threads_.clear(); }

  void resize(size_t num, TT &tt);
  void wait();
  void exec(std::function<void(Thread &)> func);
  void stop();
  bool is_busy();

  std::atomic_bool stop_; // Used by main worker and clock
private:
  std::vector<std::unique_ptr<Thread>> threads_;
};

} // namespace Lyra
