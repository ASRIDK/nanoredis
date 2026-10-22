#include "thread_pool.h"

#include <utility>

namespace nanoredis {

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        stopping_ = true;
    }
    not_empty_.notify_all();
    for (std::thread &worker : workers_) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        queue_.push_back(std::move(task));
    }
    not_empty_.notify_one();
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mu_);
            not_empty_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) {
                return;  // stopping and fully drained
            }
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        task();
    }
}

}  // namespace nanoredis
