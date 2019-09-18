#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>

namespace sync {

class semaphore {
public:
    explicit semaphore(unsigned long count = 0) noexcept
        : count_{count} 
    {}

    void post() noexcept {
        {
            std::scoped_lock lock{mutex_};
            ++count_;
        }
        cv_.notify_one();
    }

    void post(unsigned int count) noexcept {
        {
            std::scoped_lock lock{mutex_};
            count_ += count;
        }
        cv_.notify_all();
    }

    void wait() noexcept {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this] { return count_ != 0; });
        --count_;
    }

    template<class Duration>
    bool wait_for(Duration&& d) noexcept {
        std::unique_lock lock{mutex_};
        if(!cv_.wait_for(lock, d, [this] { return count_ != 0; })) 
            return false;
        --count_;
        return true;
    }

    template<class TimePoint>
    bool wait_until(TimePoint&& d) noexcept {
        std::unique_lock lock{mutex_};
        if(!cv_.wait_until(lock, d, [this] { return count_ != 0; }))
            return false;
        --count_;
        return true;
    }

private:
    std::condition_variable cv_;
    std::mutex              mutex_;
    unsigned long           count_;
};

class binary_semaphore {
public:
private:
};

class fast_semaphore {
public:
    explicit fast_semaphore(int count = 0) noexcept
        : count_{count}
    { 
        assert(count > -1); 
    }

    void post() noexcept {
        if (count_.fetch_add(1, std::memory_order_release) < 0)
            semaphore_.post();
    }

    void wait() noexcept {
        if (count_.fetch_sub(1, std::memory_order_acquire) < 1)
            semaphore_.wait();
    }
private:
    semaphore           semaphore_{0};
    std::atomic_long    count_;
};

} // namespace sync
