#pragma once

#include "event.hpp"
#include "internal/mutex.hpp"
#include "semaphore.hpp"

#include <atomic>
#include <climits>
#include <thread>
#include <utility>

namespace sync {

// Mutex Types
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

    auto native_handle() {
        return &mtx_.handle_;
    }

private:
    sync_mutex mtx_{};
};

class timed_mutex {
public:
    timed_mutex() {
        initialize_mutex(mtx_);
    }

    timed_mutex(timed_mutex const&) = delete;

    ~timed_mutex() {
        destroy_mutex(mtx_);
    }

    void lock() {
        acquire_mutex(mtx_);
    }

    bool try_lock() {
        return try_acquire_mutex(mtx_);
    }

    template<class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep, Period> const& dur) {
        return try_lock_until(std::chrono::steady_clock::now() + dur);
    }

    template<class Clock, class Duration>
    bool try_lock_until(std::chrono::time_point<Clock,Duration> const& time) {
        using namespace std::chrono;
        unique_lock lock{mtx_};
        bool no_timeout{Clock::now() < time};
        while (no_timeout && locked_)
            no_timeout = cv_.wait_until(lock, time) == cv_status::no_timeout;
        if (!locked_) {
            locked_ = true;
            return true;
        }
        return false;
    }
	
    void unlock() {
        release_mutex(mtx_);
        locked_ = false;
    }

    auto native_handle() {
        return &mtx_.handle_;
    }

private:
    sync_mutex          mtx_;
    condition_variable  cv_;
    bool                locked_{false};
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

// Lock Types

struct defer_lock_t { explicit defer_lock_t() = default; };
struct try_to_lock_t { explicit try_to_lock_t() = default; };
struct adopt_lock_t { explicit adopt_lock_t() = default; };

template<class Mutex>
class lock_guard {
public:
    using mutex_type = Mutex;

    explicit lock_guard(mutex_type& m)
        : mtx_{m}
    {
        mtx_.lock();
    }

    lock_guard(mutex_type& m, adopt_lock_t t)
        : mtx_{m}
    {}

    lock_guard(const lock_guard&) = delete;

    ~lock_guard() {
        mtx_.unlock();
    }

private:
    mutex_type& mtx_;
};

template<class Mutex>
class unique_lock {
public:
    using mutex_type = Mutex;

    // Constructors
    unique_lock() = default;
    
    unique_lock(unique_lock&&) = default;

    unique_lock(mutex_type& m)
        : mtx_{&m}
        , owns_{true}
    {
        mtx_.lock();
    }

    unique_lock(mutex_type& m, defer_lock_t t)
        : mtx_{&m}
    {}
    
    unique_lock(mutex_type& m, try_to_lock_t t) 
        : mtx_{&m}
        , owns_{mtx_.try_lock()}
    {}

    unique_lock(mutex_type& m, adopt_lock_t t) 
        : mtx_{&m}
        , owns_{true}
    {}

    template< class Rep, class Period >
    unique_lock(mutex_type& m, std::chrono::duration<Rep,Period> const& timeout_duration)
        : mtx_{&m}
        , owns_{mtx_.try_lock_for(timeout_duration)}
    {}

    template< class Clock, class Duration >
    unique_lock(mutex_type& m, std::chrono::time_point<Clock,Duration> const& timeout_time)
        : mtx_{&m}
        , owns_{mtx_try_lock_until(timeout_time)}

    // Destructor
    ~unique_lock() {
        if (owns_)
            mtx_.unlock();
    }

    // assignment operator
    unique_lock& operator=(unique_lock&& other) = default;

    // Locking
    void lock() {
        mtx_.lock();    
    }

    bool try_lock() {
        mtx_.try_lock();
    }

    template<class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep,Period> const& timeout_duration) {
        owns_ = mtx_.try_lock_for(timeout_duration);
    }

    template<class Clock, class Duration>
    bool try_lock_until(std::chrono::time_point<Clock,Duration> const& timeout_time) {
        owns_ = mtx_.try_lock_until(timeout_time);
    }

    void unlock() {
        mtx_.unlock();
        owns_ = false;
    }

    // Modifiers
    void swap(unique_lock& other) {
        std::swap(mtx_, other.mtx_);
        std::swap(owns_, other.owns_);
    }

    mutex_type* release() noexcept {
        mutex_type* mptr{std::exchange(mtx_, nullptr)};
        owns_ = false;
        return mptr;
    }

    // Observers
    mutex_type* mutex() const noexcept {
        return mtx_;
    }

    bool owns_lock() const noexcept {
        return owns_;
    }

    explicit operator bool() const noexcept {
        return owns_;
    }

private:
    Mutex*  mtx_{nullptr};
    bool    owns_{false};
};

template<typename Mutex>
inline void swap(unique_lock<Mutex>& x, unique_lock<Mutex>& y) noexcept { 
    x.swap(y); 
}

} // namespace sync
