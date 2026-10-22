#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace nanoredis {

// A fixed-size pool of worker threads consuming a FIFO task queue. The
// destructor drains the queue and joins every worker.
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    void enqueue(std::function<void()> task);

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> queue_;
    std::mutex mu_;
    std::condition_variable not_empty_;
    bool stopping_ = false;
};

}  // namespace nanoredis
