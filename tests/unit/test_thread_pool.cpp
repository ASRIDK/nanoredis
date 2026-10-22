#include <atomic>

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
