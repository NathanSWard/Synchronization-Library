// mutex_extra.hpp
#pragma once

#include "event.hpp"
#include "semaphore.hpp"
#include "stdlib/internal/sync_mutex.hpp"
#include "stdlib/thread.hpp"

#include <atomic>
#include <limits>

namespace sync {

class spinlock_mutex {
public:
    void lock() noexcept {
        while (flag_.test_and_set(std::memory_order_acquire))
            this_thread::yield();
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

    bool try_lock_shared() {
        if (rdcount_.fetch_add(-1, std::memory_order_acquire) < 1)
            return false;
        return true;
    }
    
    void unlock_shared() noexcept {
        if (rdcount_.fetch_add(1, std::memory_order_release) < 0)
            if (rdwait_.fetch_add(-1, std::memory_order_acquire) == 1)
                wrsem_.post();
    }
    
    void lock() noexcept {
        wrmtx_.lock();
        long const count{rdcount_.fetch_sub(std::numeric_limits<long>::max(), std::memory_order_acquire)};
        if (count < std::numeric_limits<long>::max()) {
            long const rdwait{rdwait_.fetch_add(std::numeric_limits<long>::max() - count, std::memory_order_acquire)};
            if (rdwait + std::numeric_limits<long>::max() - count)
                wrsem_.wait();
        }
    }

    [[nodiscard]]
    bool try_lock() {
        if (!wrmtx_.try_lock())
            return false;
        long const count{rdcount_.fetch_sub(std::numeric_limits<long>::max(), std::memory_order_acquire)};
        if (count < std::numeric_limits<long>::max()) {
            long const rdwait{rdwait_.fetch_add(std::numeric_limits<long>::max() - count, std::memory_order_acquire)};
            if (!(rdwait + std::numeric_limits<long>::max() - count))
                return true;
        }
        rdcount_.fetch_add(std::numeric_limits<long>::max(), std::memory_order_release);
        return false;
    }
    
    void unlock() noexcept {
        int const count = rdcount_.fetch_add(std::numeric_limits<long>::max(), std::memory_order_release);
        if (count < 0)
            rdsem_.post(-count);
        wrmtx_.unlock();
    }
    
private:
    semaphore       	wrsem_{0};
    semaphore       	rdsem_{0};
    fast_mutex          wrmtx_;
    std::atomic_long 	rdcount_{std::numeric_limits<long>::max()};
    std::atomic_long 	rdwait_{0};	
};

namespace os {

class rw_mutex {
public:
    rw_mutex() {
        sync_rwlock_init(mtx_);
    }

    ~rw_mutex() {
        sync_rwlock_destroy(mtx_);
    }

    void lock() {
        sync_rwlock_wrlock(mtx_);
    }

    [[nodiscard]]
    bool try_lock() {
        sync_rwlock_trywrlock(mtx_);
    }
    
    void unlock() {
        sync_rwlock_wrunlock(mtx_);
    }

    void lock_shared() {
        sync_rwlock_rdlock(mtx_);
    }

    [[nodiscard]]
    bool try_lock_shared() {
        sync_rwlock_tryrdlock(mtx_);
    }
    
    void unlock_shared() {
        sync_rwlock_rdunlock(mtx_);
    }
    
private:
    sync_rwlock_t mtx_{};
};

} // namespace os

} // namespace sync
