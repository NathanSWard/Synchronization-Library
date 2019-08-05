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
    std::atomic_flag flag_{ATOMIC_FLAG_INIT};
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

class rw_fast_mutex {
public:	
    void read_lock() {
        if (count_.fetch_sub(1, std::memory_order_acquire) < 1)
            rdwset_.wait();
    }

    // TODO: try_read_lock()
    
    void read_unlock() {
        if (count_.fetch_add(1, std::memory_order_release) < 0)
            if (rdwake_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                wrwset_.post();
    }
    
    void write_lock() {
        if (wrstate_.fetch_sub(1, std::memory_order_acquire) < 1)
            wrmtx_.wait();
        int const count = count_.fetch_sub(INT_MAX, std::memory_order_acquire);
        if (count < INT_MAX) {
            int rdwake = rdwake_.fetch_add(INT_MAX - count, std::memory_order_acquire);
            if (rdwake + INT_MAX - count)
                wrwset_.wait();
        }
    }

    // TODO: try_write_unlock()
    
    void write_unlock() {
        int const count = count_.fetch_add(INT_MAX, std::memory_order_release);
        if (count < 0)
            rdwset_.post(-count);
        if (wrstate_.fetch_add(1, std::memory_order_release) < 0)
            wrmtx_.post();
    }
    
private:
    semaphore       rdwset_{0};
    semaphore       wrwset_{0};
    semaphore       wrmtx_{0};
    std::atomic_int wrstate_{1};
    std::atomic_int count_{INT_MAX};
    std::atomic_int rdwake_{0};	
};

} // namespace sync