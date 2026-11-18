#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "../../src/thread_pool.h"
#include "test.h"

using namespace nanoredis;

TEST(thread_pool_runs_every_task_before_shutdown) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(4);
        for (int i = 0; i < 10000; ++i) {
            pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        }
    }  // destructor drains the queue
    CHECK(counter.load() == 10000);
}

TEST(thread_pool_single_worker_preserves_fifo_order) {
    std::vector<int> order;
    {
        ThreadPool pool(1);
        for (int i = 0; i < 100; ++i) {
            pool.enqueue([&order, i] { order.push_back(i); });
        }
    }
    CHECK(order.size() == 100);
    for (size_t i = 0; i < order.size(); ++i) {
        CHECK(order[i] == static_cast<int>(i));
    }
}

TEST(thread_pool_destructor_with_no_tasks_returns) {
    ThreadPool pool(3);  // must not hang or crash on shutdown
    CHECK(true);
}

TEST(thread_pool_runs_tasks_on_multiple_threads_concurrently) {
    // Two tasks rendezvous with each other; this only completes if they run
    // at the same time on two different workers.
    std::mutex mu;
    std::condition_variable cv;
    int arrived = 0;
    bool both_ran = false;
    {
        ThreadPool pool(2);
        for (int i = 0; i < 2; ++i) {
            pool.enqueue([&] {
                std::unique_lock<std::mutex> lock(mu);
                ++arrived;
                cv.notify_all();
                both_ran = cv.wait_for(lock, std::chrono::seconds(5), [&] { return arrived == 2; });
            });
        }
    }
    CHECK(both_ran);
}

TEST(thread_pool_task_may_enqueue_follow_up_work) {
    std::atomic<int> counter{0};
    std::mutex mu;
    std::condition_variable cv;
    {
        ThreadPool pool(2);
        pool.enqueue([&] {
            counter.fetch_add(1);
            pool.enqueue([&] {
                counter.fetch_add(1);
                std::lock_guard<std::mutex> lock(mu);
                cv.notify_all();
            });
        });
        std::unique_lock<std::mutex> lock(mu);
        cv.wait_for(lock, std::chrono::seconds(5), [&] { return counter.load() == 2; });
    }
    CHECK(counter.load() == 2);
}
