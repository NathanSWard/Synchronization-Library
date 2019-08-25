#pragma once

#include "internal/cv.hpp"
#include "internal/mtx.hpp"
#include <utility>

namespace sync {

// mutex types
class mutex {
public:
    /*constexpr*/ mutex() {
        initialize_mutex(mtx_);
    }

    mutex(mutex const&) = delete;

    ~mutex() {
        destroy_mutex(mtx_);
    }

    mutex& operator=(mutex const&) = delete;

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
    sync_mtx_t mtx_;
};

// lock types
struct defer_lock_t { explicit defer_lock_t() = default; };
struct try_to_lock_t { explicit try_to_lock_t() = default; };
struct adopt_lock_t { explicit adopt_lock_t() = default; };

inline constexpr defer_lock_t defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};
inline constexpr adopt_lock_t adopt_lock{};

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
        mtx_->lock();
    }

    unique_lock(mutex_type& m, defer_lock_t t)
        : mtx_{&m}
    {}
    
    unique_lock(mutex_type& m, try_to_lock_t t) 
        : mtx_{&m}
        , owns_{mtx_->try_lock()}
    {}

    unique_lock(mutex_type& m, adopt_lock_t t) 
        : mtx_{&m}
        , owns_{true}
    {}

    template< class Rep, class Period >
    unique_lock(mutex_type& m, std::chrono::duration<Rep,Period> const& timeout_duration)
        : mtx_{&m}
        , owns_{mtx_->try_lock_for(timeout_duration)}
    {}

    template< class Clock, class Duration >
    unique_lock(mutex_type& m, std::chrono::time_point<Clock,Duration> const& timeout_time)
        : mtx_{&m}
        , owns_{mtx_->try_lock_until(timeout_time)}
    {}

    // Destructor
    ~unique_lock() {
        if (owns_)
            mtx_->unlock();
    }

    // assignment operator
    unique_lock& operator=(unique_lock&& other) {
        mtx_ = std::exchange(other.mtx_, nullptr);
        owns_ = std::exchange(other.owns_, false);
    }

    // Locking
    void lock() {
        mtx_->lock();    
    }

    bool try_lock() {
        mtx_->try_lock();
    }

    template<class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep,Period> const& timeout_duration) {
        owns_ = mtx_->try_lock_for(timeout_duration);
    }

    template<class Clock, class Duration>
    bool try_lock_until(std::chrono::time_point<Clock,Duration> const& timeout_time) {
        owns_ = mtx_->try_lock_until(timeout_time);
    }

    void unlock() {
        mtx_->unlock();
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
    mutex_type*     mtx_{nullptr};
    bool            owns_{false};
};

template<typename Mutex>
inline void swap(unique_lock<Mutex>& x, unique_lock<Mutex>& y) noexcept { 
    x.swap(y); 
}

enum class cv_status {
    no_timeout,
    timeout
};

// condition variable types
class condition_variable {
public:
    condition_variable() {
        initialize_cv(cv_);
    }

    condition_variable(condition_variable const&) = delete;

    void notify_one() noexcept {
        signal_cv(cv_);
    }

    void notify_all() noexcept {
        broadcast_cv(cv_);
    }

    void wait(unique_lock<mutex>& lock) {
        wait_cv(cv_, *lock.mutex()->native_handle());
    }

    template<class Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        while (!pred())
            wait(lock);
    }

    template<class Rep, class Period>
    cv_status wait_for(unique_lock<mutex>& lock, 
                        const std::chrono::duration<Rep, Period>& dur) {
        if (dur <= dur.zero())
            return cv_status::timeout;
        using sys_tpf = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long double, std::nano>>;
        using sys_tpi = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
        sys_tpf max{sys_tpi::max()};
        std::chrono::steady_clock::time_point c_now{std::chrono::steady_clock::now()};
        std::chrono::system_clock::time_point s_now{std::chrono::system_clock::now()};
        if (max - dur > s_now)
            timed_wait_cv(cv_, *lock.mutex()->native_handle(), s_now + std::chrono::ceil<std::chrono::nanoseconds>(dur));
        else
            timed_wait_cv(cv_, *lock.mutex()->native_handle(), sys_tpi::max());
        
        return std::chrono::steady_clock::now() - c_now < dur ? cv_status::no_timeout : cv_status::timeout;
    }

    template<class Rep, class Period, class Predicate>
    bool wait_for(unique_lock<mutex>& lock,
                    const std::chrono::duration<Rep, Period>& dur,
                    Predicate pred) {
        return wait_until(lock, std::chrono::steady_clock::now() + dur, std::move(pred));
    }

    template<class Clock, class Duration>
    cv_status wait_until(unique_lock<mutex>& lock,
                                std::chrono::time_point<Clock, Duration> const& time) {
        using namespace std::chrono;
        wait_for(lock, time - Clock::now());
        return Clock::now() < time ? cv_status::no_timeout : cv_status::timeout;
    }

    template<class Clock, class Duration, class Pred>
    bool wait_until(unique_lock<mutex>& lock,
                    std::chrono::time_point<Clock, Duration> const& time,
                    Pred pred) {
        while (!pred()) {
            if (wait_until(lock, time) == cv_status::timeout)
                return pred();
        }
        return true;
    }    

    auto native_handle() {
        return &cv_;
    }

    private:
        sync_cv_t cv_;
};

}