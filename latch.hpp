#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace sync {

    class latch {
    public:
        explicit latch(std::ptrdiff_t n) noexcept
            : count_{n}
        {}

        latch(latch const&) = delete;

        void count_down_and_wait() noexcept {
            if (count_.fetch_sub(1, std::memory_order_acquire) == 0) {
                cv_.notify_all;
                return;
            }
            
            std::unique_lock lock{mtx_};
            cv_.wait(lock, [&]{return is_ready();});
        }

        void count_down(std::ptrdiff_t n = 1) noexcept {
            count_.fetch_sub(1, std::memory_order_release);
        }
        
        bool is_ready() const noexcept {
            return count_.load(std::memory_order_acquire) == 0;
        }

        void wait() const noexcept {
            if (is_ready())
                return;

            std::unique_lock lock{mtx_};
            cv_.wait(lock, [&]{return is_ready();});
        }

    private:
        std::condition_variable mutable cv_;
        std::mutex mutable              mtx_;
        std::atomic_ptrdiff_t           count_;
    };
    
}