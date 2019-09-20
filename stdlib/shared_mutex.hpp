// shared_mutex.hpp

#include "condition_variable.hpp"
#include "mutex.hpp"

namespace sync {

struct shared_mutex_base {
    mutex               mtx_;
    condition_variable  gate1_;
    condition_variable  gate2_;
    unsigned            state_;

    static constexpr unsigned write_entered_ = 1U << (sizeof(unsigned)*__CHAR_BIT__ - 1);
    static constexpr unsigned n_readers_ = ~write_entered_;

    shared_mutex_base();
    ~shared_mutex_base() = default;

    shared_mutex_base(shared_mutex_base const&) = delete;
    shared_mutex_base& operator=(shared_mutex_base const&) = delete;

    // Exclusive ownership
    void lock(); // blocking
    bool try_lock();
    void unlock();

    // Shared ownership
    void lock_shared(); // blocking
    bool try_lock_shared();
    void unlock_shared();
};

class shared_mutex {
    shared_mutex_base base_;
public:
    shared_mutex() : base_{} {}
    ~shared_mutex() = default;

    shared_mutex(shared_mutex const&) = delete;
    shared_mutex& operator=(shared_mutex const&) = delete;

    // Exclusive ownership
    void lock() { base_.lock(); }
    bool try_lock() { return base_.try_lock(); }
    void unlock() { base_.unlock(); }

    // Shared ownership
    void lock_shared() { base_.lock_shared(); }
    bool try_lock_shared() { base_.try_lock_shared(); }
    void unlock_shared() { base_.unlock_shared(); }

    // auto native_handle();
};

class shared_timed_mutex {
    shared_mutex_base base_;
public:
    shared_timed_mutex();
    ~shared_timed_mutex() = default;

    shared_timed_mutex(const shared_timed_mutex&) = delete;
    shared_timed_mutex& operator=(const shared_timed_mutex&) = delete;

    // Exclusive ownership
    void lock();
    bool try_lock();
    template <class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep, Period>const& rel_time) {
            return try_lock_until(std::chrono::steady_clock::now() + rel_time);
    }
    
    template <class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time);
    void unlock();

    // Shared ownership
    void lock_shared();
    bool try_lock_shared();
    template <class Rep, class Period>
    bool try_lock_shared_for(std::chrono::duration<Rep, Period> const& rel_time) {
            return try_lock_shared_until(chrono::steady_clock::now() + rel_time);
    }
    
    template <class Clock, class Duration>
    bool try_lock_shared_until(std::chrono::time_point<Clock, Duration> const& abs_time);
    void unlock_shared();
};

template <class Clock, class Duration>
bool shared_timed_mutex::try_lock_until(std::chrono::time_point<Clock, Duration> const& abs_time) {
    unique_lock lock{base_.mtx_};
    if (base_.state_ & base_.write_entered_) {
        for (;;) {
            cv_status status{base_.gate1_.wait_until(lock, abs_time)};
            if ((base_.state_ & base_.write_entered_) == 0)
                break;
            if (status == cv_status::timeout)
                return false;
        }
    }
    base_.state_ |= base_.write_entered_;
    if (base_.state_ & base_.n_readers_) {
        for (;;) {
            cv_status status{base_.gate2_.wait_until(lock, abs_time)};
            if ((base_.state_ & base_.n_readers_) == 0)
                break;
            if (status == cv_status::timeout) {
                base_.state_ &= ~base_.write_entered_;
                base_.gate1_.notify_all();
                return false;
            }
        }
    }
    return true;
}

template <class Clock, class Duration>
bool shared_timed_mutex::try_lock_shared_until(std::chrono::time_point<Clock, Duration> const& abs_time) {
    unique_lock<mutex> lock{base_.mtx_};
    if ((base_.state_ & base_.write_entered_) || (base_.state_ & base_.n_readers_) == base_.n_readers_) {
        for (;;) {
            cv_status status{base_.gate1_.wait_until(lock, abs_time)};
            if ((base_.state_ & base_.write_entered_) == 0 && (base_.state_ & base_.n_readers_) < base_.n_readers_)
                break;
            if (status == cv_status::timeout)
                return false;
        }
    }
    unsigned const num_readers{(base_.state_ & base_.n_readers_) + 1};
    base_.state_ &= ~base_.n_readers_;
    base_.state_ |= num_readers;
    return true;
}

template<class Mutex>
class shared_lock {
public:
    using mutex_type = Mutex;
private:
    mutex_type* mtx_{nullptr};
    bool owns_{false};
public:
    shared_lock() noexcept {}
    
    explicit shared_lock(mutex_type& mtx)
        : mtx_{std::addressof(mtx)}
    {
        mtx_->lock_shared();
    }

    shared_lock(mutex_type& mtx, defer_lock_t) noexcept 
        : mtx_{std::addressof(mtx)}

    {}

    shared_lock(mutex_type& mtx, try_to_lock) 
        : mtx_{std::addressof(mtx)}
        , owns_{mtx.try_lock_shared()}

    {}

    shared_lock(mutex_type& mtx, defer_lock_t) noexcept 
        : mtx{std::addressof(mtx)}
        , owns_{true}
    {}

    template <class Rep, class Period>
    shared_lock(mutex_type& mtx,
                chrono::duration<_Rep, _Period> const& rel_time)
        : mtx_(std::addressof(mtx))
        , owns_(mtx.try_lock_shared_for(rel_time))
    {}

    ~shared_lock() {
        if (owns_)
            mtx_->unlock_shared();
    }

    shared_lock(shared_lock const&) = delete;
    shared_lock& operator=(shared_lock const&) = delete;

    shared_lock(shared_lock&& other) noexcept 
        : mtx_{other.mtx_}
        , owns_{other.owns_}
    {
        other.mtx_ = nullptr;
        other.owns_ = false;
    }

    shared_lock& operator=(shared_lock&& ohter) noexcept {
        if (owns_)
            mtx_.unlock_shared();
        mtx_ = std::exchange(other.mtx_, nullptr);
        owns_ = std::exchange(other.owns_, false);
    }

    void lock() {
        if (mtx_ == nullptr)
            ; // throw_system_error
        if (owns_)
            ; //throw_system_error
        mtx_->lock_shared();
        owns_ = true;
    }

    bool try_lock() {
         if (mtx_ == nullptr)
            ; // throw_system_error
        if (owns_)
            ; //throw_system_error
        owns_ = mtx_->try_lock_shared();
        return owns_;       
    }
    
    template<class Rep, class Period>
    bool try_lock_for(std::chrono::duration<Rep, Period> const& rel_time) {
         if (mtx_ == nullptr)
            ; // throw_system_error
        if (owns_)
            ; //throw_system_error
        owns_ = mtx_->try_lock_shared_for(rel_time);
        return owns_;      
    }
    
    template<class Clock, class Dur>
    bool try_lock_until(std::chrono::time_point<Clock, Dur> const& abs_time) {
        if (mtx_ == nullptr)
            ; // throw_system_error
        if (owns_)
            ; //throw_system_error
        owns_ = mtx_->try_lock_shared_until(abs_time);
        return owns_;             
    }
    
    void unlock() {
        if (!owns_)
            ; //throw_system_error
        mtx_->unlock_shared();
        owns_ = false;
    }

    void swap(shared_lock& other) noexcept {
        std::swap(mtx_, other.mtx_);
        std::swap(owns_, other.owns_);
    }

    mutex_type* release() noexcept {
        mutex_type* mtx{mtx_};
        mtx_ = nullptr;
        owns_ = false;
        return mtx;
    } 

    bool owns_lock() const noexcept {
        return owns_;
    }

    explicit operator bool() const noexcept {
        return owns_;
    }

    mutex_type* mutex() const noexcept {
        return mtx_;
    }
};

template<class Mutex>
inline void swap(shared_lock<Mutex>& a, shared_lock<Mutex>& b) noexcept {
    a.swap(b);
}

}