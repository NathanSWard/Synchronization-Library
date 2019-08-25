#pragma once

#include "_mutex_base.hpp"

namespace sync {

// Mutex Types

class recursive_mutex {
public:
    recursive_mutex() {
        initialize_mutex(mtx_);
    }

    recursive_mutex(recursive_mutex const&) = delete;

    ~recursive_mutex() {
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
        return &mtx_;
    }

private:
    sync_mtx_t mtx_ = SYNC_RECURSIVE_MTX_INIT;
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
        return &mtx_;
    }

private:
    sync_mtx_t          mtx_;
    condition_variable  cv_;
    bool                locked_{false};
};

// class recursive_timed_mutex;

// try_lock()

// lock()

// class scoped_lock;

// class call_once;

}
