#pragma once

#include <chrono>
#include "internal/cv.hpp"
#include "mutex.hpp"
#include <type_traits>

namespace sync {

enum class cv_status {
    no_timeout,
    timeout
};

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
        wait_cv(cv_, lock.mutex->native_handle());
    }

    void wait(unique_lock<mutex>& lock) {
        wait_cv(cv_, lock.mutex->native_handle());
    }

    void wait(unique_lock<mutex>& lock) {
        wait_cv(cv_, lock.mutex->native_handle());
    }

    template<class Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        while (!pred())
            wait(lock);
    }

    template<class Rep, class Period>
    cv_status wait_for(unique_lock<mutex>& lock, 
                        const std::chrono::duration<Rep, Period>& dur) {
        using namespace std::chrono;
        if (dur <= dur.zero())
            return cv_status::timeout;
        using sys_tpf = time_point<system_clock, duration<long double, nano>>;
        using sys_tpi = time_point<system_clock, nanoseconds>;
        sys_tpf max{sys_tpi::max()};
        steady_clock::time_point c_now{steady_clock::now()};
        system_clock::time_point s_now{system_clock::now()};
        if (max - dur > s_now)
            timed_wait_cv(&cv_, *lock.mutex()->native_handle(), s_now + ceil<nanoseconds>(dur));
        else
            timed_wait_cv(&cv_, *lock.mutex()->native_handle(), sys_tpi::max())
        
        return steady_clock::now() - c_now < dur ? cv_status::no_timeout : cv_status::timeout;
    }

    template<class Rep, class Period, class Predicate>
    bool wait_for(unique_lock<mutex>& lock,
                    const std::chrono::duration<Rep, Period>& dur,
                    Predicate pred) {
        return wait_until(lock, std::chrono::steady_clock::now() + dur, std::move(pred));
    }

    template<class Clock, class Duration>
    std::cv_status wait_until(unique_lock<mutex>& lock,
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

    auto& native_handle() {
        return cv_.handle_;
    }

    private:
        sync_condition_variable cv_;
};

}