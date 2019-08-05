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
        if (count_.fetch_sub(1, std::memory_order_acquire) < 1)
            rdwset_.wait();
    }

    // TODO: try_read_lock()
    
    void unlock_shared() {
        if (count_.fetch_add(1, std::memory_order_release) < 0)
            if (rdwake_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                wrwset_.post();
    }
    
    void lock() {
        if (wrstate_.fetch_sub(1, std::memory_order_acquire) < 1)
            wrmtx_.wait();
        int const count = count_.fetch_sub(LONG_MAX, std::memory_order_acquire);
        if (count < LONG_MAX) {
            int rdwake = rdwake_.fetch_add(LONG_MAX - count, std::memory_order_acquire);
            if (rdwake + LONG_MAX - count)
                wrwset_.wait();
        }
    }

    // TODO: try_write_unlock()
    
    void unlock() {
        int const count = count_.fetch_add(LONG_MAX, std::memory_order_release);
        if (count < 0)
            rdwset_.post(-count);
        if (wrstate_.fetch_add(1, std::memory_order_release) < 0)
            wrmtx_.post();
    }
    
private:
    semaphore       	rdwset_{0};
    semaphore       	wrwset_{0};
    semaphore       	wrmtx_{0};
    std::atomic_long 	wrstate_{1};
    std::atomic_long 	count_{LONG_MAX};
    std::atomic_long 	rdwake_{0};	
};

} // namespace sync
