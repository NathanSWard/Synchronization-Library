// latch.hpp
#pragma once

#include "condition_variable.hpp"
#include "mutex.hpp"

#include <atomic>

namespace sync {

class latch {
public:
    explicit latch(std::ptrdiff_t n)
        : count_{n}
    {}

    latch(latch const&) = delete;
    latch& operator=(latch const&) = delete;

    void count_down_and_wait() {
        if (count_.fetch_sub(1, std::memory_order_acquire) == 0) {
            cv_.notify_all();
            return;
        }
        
        unique_lock lock{mtx_};
        cv_.wait(lock, [&]{return is_ready();});
    }

    void count_down(std::ptrdiff_t n = 1) {
        count_.fetch_sub(1, std::memory_order_release);
    }
    
    [[nodiscard]]
    bool is_ready() const noexcept {
        return count_.load(std::memory_order_acquire) == 0;
    }

    void wait() const {
        if (is_ready())
            return;

        unique_lock lock{mtx_};
        cv_.wait(lock, [&]{return is_ready();});
    }

private:
    condition_variable mutable cv_;
    mutex mutable              mtx_;
    std::atomic_ptrdiff_t      count_;
};
    
}
