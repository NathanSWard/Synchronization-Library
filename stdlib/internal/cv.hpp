#pragma once

#include <chrono>
#include "platform.hpp"
#include "types.hpp"

namespace sync {

void initialize_cv(sync_cv_t&);
void destroy_cv(sync_cv_t&);
template<class Mutex>
void wait_cv(sync_cv_t&, Mutex&);
template<class Mutex>
void timed_wait_cv(sync_cv_t&, Mutex&, 
                    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>);
void signal_cv(sync_cv_t&);
void broadcast_cv(sync_cv_t&);

#if SYNC_WINDOWS

    void initialize_cv(sync_cv_t& cv) {
        InitializeConditionVariable(cv);
    }

    void destroy_cv(sync_cv_t&) {}

    template<class Mutex>
    void wait_cv(sync_cv_t& cv, Mutex& mtx) {
        if constexpr (std::is_same_v<Mutex, CRITICAL_SECTION>) {
            SleepConditionVariableCS(&cv, &mtx, INFINITE);
        } else if (std::is_same_v<Mutex, SRWLock>) {
            SleepConditionVariableSRW(&cv, &mtx, INFINITE, 0);
        } else {
            static_assert(false, "Windows wait_cv only takes a SRWLock or Cricial Section");
        }
    }

    template<class Mutex>
    void timed_wait_cv(sync_cv_t& cv, Mutex& mtx, 
                        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp) {
        auto duration{std::chrono::duration_cast<milliseconds>(tp.time_since_epoch()).count()};
        if constexpr (std::is_same_v<Mutex, CRITICAL_SECTION>) {
            SleepConditionVariableCS(&cv, &mtx, static_cast<DWORD>(duration));
        } else if (std::is_same_v<Mutex, SRWLock>) {
            SleepConditionVariableSRW(&cv, &mtx, static_cast<DWORD>(duration), 0);
        } else {
            static_assert(false, "Windows timed_wait_cv only takes a SRWLock or Cricial Section");
        }
    }

    void signal_cv(sync_cv_t& cv) {
        WakeConditionVariable(&cv);
    }

    void broadcast_cv(sync_cv_t& cv) {
        WakeAllConditionVariable(&cv);
    }

#elif SYNC_MAX || SYNC_LINUX

    void initialize_cv(sync_cv_t& cv) {
        pthread_cond_init(&cv, nullptr);
    }

    void destroy_cv(sync_cv_t& cv) {
        pthread_cond_destroy(&cv);
    }

    template<class Mutex>
    void wait_cv(sync_cv_t& cv, Mutex& mtx) {
        pthread_cond_wait(&cv, &mtx);
    }

    template<class Mutex>
    void timed_wait_cv(sync_cv_t& cv, Mutex& mtx, 
                        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp) {
        using namespace std::chrono;
        nanoseconds d{tp.time_since_epoch()};
        if (d > nanoseconds(0x59682F000000E941))
            d = nanoseconds(0x59682F000000E941);
        ::timespec ts;
        seconds s{duration_cast<seconds>(d)};
        typedef decltype(ts.tv_sec) ts_sec;
        constexpr ts_sec ts_sec_max{std::numeric_limits<ts_sec>::max()};
        if (s.count() < ts_sec_max) {
            ts.tv_sec = static_cast<ts_sec>(s.count());
            ts.tv_nsec = static_cast<decltype(ts.tv_nsec)>((d - s).count());
        }
        else {
            ts.tv_sec = ts_sec_max;
            ts.tv_nsec = std::giga::num - 1;
        }
        pthread_cond_timewait(&cv, &mtx, &ts);
    }

    void signal_cv(sync_cv_t& cv) {
        pthread_cond_signal(&cv);
    }

    void broadcast_cv(sync_cv_t& cv) {
        pthread_cond_broadcast(&cv);
    }

#endif

}
