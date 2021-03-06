// mutex.hpp
#pragma once

#include "_mutex.hpp"
#include "thread.hpp"

#include <atomic>
#include <chrono>
#include <limits>
#include <thread>
#include <tuple>
#include <utility>

namespace sync {

// Mutex Types

class recursive_mutex {
public:
    recursive_mutex() = default;

    recursive_mutex(recursive_mutex const&) = delete;
    recursive_mutex& operator=(recursive_mutex const&) = delete;

    ~recursive_mutex() {
        sync_mutex_destroy(mtx_);
    }

    void lock() {
        sync_mutex_lock(mtx_);
    }

    bool try_lock() {
        return sync_mutex_trylock(mtx_);
    }

    void unlock() {
        sync_mutex_unlock(mtx_);
    }

    auto native_handle() {
        return &mtx_;
    }

private:
    sync_mutex_t mtx_ = SYNC_RECURSIVE_MUTEX_INIT;
};

class timed_mutex {
public:
    timed_mutex() = default;

    timed_mutex(timed_mutex const&) = delete;
    timed_mutex& operator=(timed_mutex const&) = delete;

    ~timed_mutex() {
        lock_guard lock{mtx_};
    }

    void lock() {
        unique_lock lock{mtx_};
        while (locked_)
            cv_.wait(lock);
        locked_ = true;
    }

    bool try_lock() {
        unique_lock lock{mtx_, try_to_lock};
        if (lock.owns_lock() && !locked_) {
            locked_ = true;
            return true;
        }
        return false;
    }

    template<class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep, Period> const& dur) {
        return try_lock_until(std::chrono::steady_clock::now() + dur);
    }

    template<class Clock, class Duration>
    bool try_lock_until(std::chrono::time_point<Clock, Duration> const& time) {
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
        lock_guard lock{mtx_};
        locked_ = false;
        cv_.notify_one();
    }

    auto native_handle() {
        return mtx_.native_handle();
    }

private:
    mutex               mtx_;
    condition_variable  cv_;
    bool                locked_{false};
};

class recursive_timed_mutex {
public:
    recursive_timed_mutex() = default;

    ~recursive_timed_mutex() {
        lock_guard lock{mtx_};
    }

    recursive_timed_mutex(timed_mutex const&) = delete;
    recursive_timed_mutex& operator=(timed_mutex const&) = delete;

    void lock() {
        auto id = this_thread::get_id();
        unique_lock lock{mtx_};
        if (id == id_) {
            if (count_ == std::numeric_limits<std::size_t>::max())
                SYNC_ASSERT(false, "recursive_timed_mutex lock limit reached");
            ++count_;
            return;
        }
        while (count_ != 0)
            cv_.wait(lock);
        count_ = 1;
        id_ = id;
    }

    bool try_lock() noexcept {
        auto id = this_thread::get_id();
        unique_lock lock{mtx_, try_to_lock};
        if (lock.owns_lock() && (count_ == 0 || id == id_)) {
            if (count_ == std::numeric_limits<std::size_t>::max())
                return false;
            ++count_;
            id_ = id;
            return true;
        }
        return false;
    }

    template<class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep, Period> const& dur) {
        return try_lock_until(std::chrono::steady_clock::now() + dur);
    }

    template<class Clock, class Duration>
    bool try_lock_until(std::chrono::time_point<Clock,Duration> const& time) {
        using namespace std::chrono;
        auto id = this_thread::get_id();
        unique_lock lock{mtx_};
        if (id == id_) {
            if (count_ == std::numeric_limits<std::size_t>::max())
                return false;
            ++count_;
            return true;
        }
        bool no_timeout = Clock::now() < time;
        while (no_timeout && count_ != 0)
            no_timeout = cv_.wait_until(lock, time) == cv_status::no_timeout;
        if (count_ == 0) {
            count_ = 1;
            id_ = id;
            return true;
        }
        return false;
    }
	
    void unlock() {
        unique_lock lock{mtx_};
        if (--count_ == 0) {
            id_ = SYNC_NULL_THREAD;
            lock.unlock();
            cv_.notify_one();
        }
    }

    auto native_handle() {
        return &mtx_;
    }

private:
    mutex               mtx_;
    condition_variable  cv_;
    std::size_t         count_{0};
    sync_thread_id_t    id_{}; 
};

// locking functions

template<class M0, class M1>
int try_lock(M0& m0, M1& m1) {
    unique_lock lock{m0, try_to_lock};
    if (lock.owns_lock()) {
        if (m1.try_lock()) {
            lock.release();
            return -1;
        }
        else
            return 1;
    }
    return 0;
}

template<class M0, class M1, class M2, class... MN>
int try_lock(M0& m0, M1& m1, M2& m2, MN&... mn) {
    int i{0};
    unique_lock lock{m0, try_to_lock};
    if (lock.owns_lock()) {
        i = try_lock(m1, m2, mn...);
        if (i == -1)
            lock.release();
        else
            ++i;
    }
    return i;
}

template<class M0, class M1>
void lock(M0& m0, M1& m1) {
    for (;;) {
        {
            unique_lock lock{m0};
            if (m1.try_lock()) {
                lock.release();
                break;
            }
        }
        this_thread::yield();
        {
            unique_lock lock{m1};
            if (m0.try_lock()) {
                lock.release();
                break;
            }
        }
        this_thread::yield();
    }
}

template<class M0, class M1, class M2, class ...MN>
void _lock_first(int i, M0& m0, M1& m1, M2& m2, MN& ...mn) {
    for (;;) {
        switch (i) {
            case 0:
                {
                    unique_lock lock{m0};
                    i = try_lock(m1, m2, mn...);
                    if (i == -1) {
                        lock.release();
                        return;
                    }
                }
                ++i;
                this_thread::yield();
                break;
            case 1:
                {
                    unique_lock lock{m1};
                    i = try_lock(m2, mn..., m0);
                    if (i == -1) {
                        lock.release();
                        return;
                    }
                }
                if (i == sizeof...(MN) + 1)
                    i = 0;
                else
                    i += 2;
                this_thread::yield();
                break;
            default:
                _lock_first(i - 2, m2, mn..., m0, m1);
                return;
        }
    }
}

template<class M0, class M1, class M2, class ...MN>
inline void lock(M0& m0, M1& m1, M2& m2, MN& ...mn) {
    _lock_first(0, m0, m1, m2, mn...);
}

template<class M0>
inline void _unlock(M0& m0) {
    m0.unlock();
}

template<class M0, class M1>
inline void _unlock(M0& m0, M1& m1) {
    m0.unlock();
    m1.unlock();
}

template<class M0, class M1, class M2, class ...MN>
inline void _unlock(M0& m0, M1& m1, M2& m2, MN&... mn) {
    m0.unlock();
    m1.unlock();
    _unlock(m2, mn...);
}

// scoped_lock

template<class ...MutexTypes>
class scoped_lock;

template<>
class scoped_lock<> {
public:
    explicit scoped_lock() {}
    ~scoped_lock() = default;

    explicit scoped_lock(adopt_lock_t) {}

    scoped_lock(scoped_lock const&) = delete;
    scoped_lock& operator=(scoped_lock const&) = delete;
};

template<typename Mutex>
class scoped_lock<Mutex> {
public:
    using mutex_type = Mutex;

    explicit scoped_lock(mutex_type& mtx)
        : mtx_{mtx}
    {
        mtx_.lock();
    }

    explicit scoped_lock(adopt_lock_t, mutex_type& mtx)
        : mtx_{mtx}
    {}

    scoped_lock(scoped_lock const&) = delete;

    ~scoped_lock() {
        mtx_.unlock();
    }

    scoped_lock& operator=(scoped_lock const&) = delete;

private:
    mutex_type& mtx_;
};

template<class... MutexTypes>
class scoped_lock {
    static_assert(sizeof...(MutexTypes) > 1, "At least 2 mutex types required.");

public:
    explicit scoped_lock(MutexTypes&... mtxs)
        : mtxs_{mtxs...}
    {
        lock(mtxs...);
    }

    scoped_lock(adopt_lock_t, MutexTypes&... mtxs)
        : mtxs_{mtxs...}
    {}

    scoped_lock(scoped_lock const&) = delete;

    ~scoped_lock() {
        std::apply(_unlock<MutexTypes...>, mtxs_);
    }

    scoped_lock& operator=(scoped_lock const&) = delete;

private:
    std::tuple<MutexTypes&...> mtxs_;
};

class once_flag {
public:
    constexpr once_flag() noexcept = default;

    once_flag(once_flag const&) = delete;
    once_flag& operator=(once_flag const&) = delete;
    
    std::atomic<bool> flag_{false};
};

template<class Callable, class... Args>
void call_once(once_flag& flag, Callable&& f, Args&&... args) {
    bool called{false};
    if (flag.flag_.compare_exchange_strong(called, true, std::memory_order_release, std::memory_order_acquire))
        std::forward<Callable>(f)(std::forward<Args>(args)...);
}

}
