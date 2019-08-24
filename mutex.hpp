#pragma once

#include "event.hpp"
#include "internal/mutex.hpp"
#include "semaphore.hpp"

#include <atomic>
#include <climits>
#include <thread>

namespace sync {

class mutex {
public:
    mutex() {
        initialize_mutex(mtx_);
    }

    mutex(mutex const&) = delete;

    ~mutex() {
        destroy_mutex(mtx_);
    }

    void lock() {
        acquire_mutex(mtx_);
    }

    bool try_lock() {
        return try_acquire_mutex(mtx_);
    }

    void unlock() {
        release_mutex(mtx_);
    }

    auto& native_handle() {
        return mtx_.handle_;
    }

private:
    sync_mutex mtx_{};
};

class spinlock_mutex {
public:
    void lock() noexcept {
        while (flag_.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
    }

    [[nodiscard]]
    bool try_lock() noexcept {
        if (flag_.test_and_set(std::memory_order_acquire))
            return false;
        return true;
    }
    
    void unlock() noexcept {
        flag_.clear(std::memory_order_release);
    }
    
private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

class fast_mutex {
public:
    void lock() noexcept {
        if (state_.exchange(1, std::memory_order_acquire))
            while (state_.exchange(2, std::memory_order_acquire))
                event_.wait();
    }

    [[nodiscard]]
    bool try_lock() noexcept {
        unsigned int expected = 0;
        if (state_.compare_exchange_weak(expected, 1, std::memory_order_acquire))
            return true;
        return false;
    }
    
    void unlock() noexcept {
        if (state_.exchange(0, std::memory_order_release) == 2)
            event_.signal();
    }
    
private:
    std::atomic_uint    state_{0};
    auto_event          event_;
};

class fast_shared_mutex {
public:	
    void lock_shared() noexcept {
        if (rdcount_.fetch_add(-1, std::memory_order_acquire) < 1)
            rdsem_.wait();
    }

    // TODO: try_lock_shared()
    
    void unlock_shared() noexcept {
        if (rdcount_.fetch_add(1, std::memory_order_release) < 0)
            if (rdwait_.fetch_add(-1, std::memory_order_acquire) == 1)
                wrsem_.post();
    }
    
    void lock() noexcept {
        wrmtx_.lock();
        long const count = rdcount_.fetch_add(-LONG_MAX, std::memory_order_acquire);
        if (count < LONG_MAX) {
            long const rdwait = rdwait_.fetch_add(LONG_MAX - count, std::memory_order_acquire);
            if (rdwait + LONG_MAX - count)
                wrsem_.wait();
        }
    }

    // TODO: try_lock()
    
    void unlock() noexcept {
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


class rw_mutex {
    public:
        rw_mutex() {
            initialize_mutex(mtx_);
        }

        void lock() {
            acquire_mutex_wr(mtx_);
        }

        bool try_lock() {
            try_acquire_mutex_wr(mtx_);
        }
        
        void unlock() {
            release_mutex_wr(mtx_);
        }
    
        void lock_shared() {
            acquire_mutex_rd(mtx_);
        }

        bool try_lock_shared() {
            try_acquire_mutex_rd(mtx_);
        }
        
        void unlock_shared() {
            release_mutex_rd(mtx_);
        }
        
    private:
        sync_rw_mutex mtx_{};
};

} // namespace sync
