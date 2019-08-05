#pragma once

#include "event.hpp"
#include "semaphore.hpp"

#include <atomic>
#include <climits>
#include <thread>

namespace sync {

class spinlock_mutex {
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
    }

    [[nodiscard]]
    bool try_lock()  {
        if (flag_.test_and_set(std::memory_order_acquire))
            return false;
        return true;
    }
    
    void unlock() {
        flag_.clear(std::memory_order_release);
    }
    
private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

class fast_mutex {
public:
    void lock() {
        if (state_.exchange(1, std::memory_order_acquire))
            while (state_.exchange(2, std::memory_order_acquire))
                event_.wait();
    }

    [[nodiscard]]
    bool try_lock() {
        unsigned int expected = 0;
        if (state_.compare_exchange_weak(expected, 1, std::memory_order_acquire))
            return true;
        return false;
    }
    
    void unlock() {
        if (state_.exchange(0, std::memory_order_release) == 2)
            event_.signal();
    }
    
private:
    std::atomic_uint    state_{0};
    auto_event          event_;
};

class fast_shared_mutex {
public:	
    void lock_shared() {
        if (rdcount_.fetch_add(-1, std::memory_order_acquire) < 1)
            rdsem_.wait();
    }

    // TODO: try_read_lock()
    
    void unlock_shared() {
        if (rdcount_.fetch_add(1, std::memory_order_release) < 0)
            if (rdwait_.fetch_add(-1, std::memory_order_acquire) == 1)
                wrsem_.post();
    }
    
    void lock() {
        wrmtx_.lock();
        long const count = rdcount_.fetch_add(-LONG_MAX, std::memory_order_acquire);
        if (count < LONG_MAX) {
            long const rdwait = rdwait_.fetch_add(LONG_MAX - count, std::memory_order_acquire);
            if (rdwait + LONG_MAX - count)
                wrsem_.wait();
        }
    }

    // TODO: try_write_unlock()
    
    void unlock() {
        int const count = rdcount_.fetch_add(LONG_MAX, std::memory_order_release);
        if (count < 0)
            rdsem_.post(-count);
        wrmtx_.unlock();
    }
    
private:
    semaphore       	wrsem_{0};
    semaphore       	rdsem_{0};
    fast_mutex          wrmtx_;
    std::atomic_long 	rdcount_{LONG_MAX};
    std::atomic_long 	rdwait_{0};	
};

} // namespace sync
