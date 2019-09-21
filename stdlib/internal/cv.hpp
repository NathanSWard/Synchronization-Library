#pragma once

#include <chrono>
#include "include/assert.hpp"
#include "include/types.hpp"

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
            SYNC_WINDOWS_ASSERT(SleepConditionVariableCS(&cv, &mtx, INFINITE), "SleepConditionVariableCS failed");
        } else if (std::is_same_v<Mutex, SRWLock>) {
            SYNC_WINDOWS_ASSERT(SleepConditionVariableSRW(&cv, &mtx, INFINITE, 0), "SleepConditionVariableSRW failed");
        } else {
            static_assert(false, "Windows wait_cv only takes a SRWLock or Cricial Section");
        }
    }

    template<class Mutex>
    void timed_wait_cv(sync_cv_t& cv, Mutex& mtx, 
                        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp) {
        auto duration{std::chrono::duration_cast<milliseconds>(tp.time_since_epoch()).count()};
        if constexpr (std::is_same_v<Mutex, CRITICAL_SECTION>) {
            SYNC_WINDOWS_ASSERT(SleepConditionVariableCS(&cv, &mtx, static_cast<DWORD>(duration)), "SleepConditionVariableCS failed");
        } else if (std::is_same_v<Mutex, SRWLock>) {
            SYNC_WINDOWS_ASSERT(SleepConditionVariableSRW(&cv, &mtx, static_cast<DWORD>(duration), 0), "SleepConditionVariableSRW failed");
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
        SYNC_POSIX_ASSERT(pthread_cond_init(&cv, nullptr), "pthread_cond_init failed");
    }

    void destroy_cv(sync_cv_t& cv) {
        SYNC_POSIX_ASSERT(pthread_cond_destroy(&cv), "pthread_cond_destroy failed");
    }

    template<class Mutex>
    void wait_cv(sync_cv_t& cv, Mutex& mtx) {
        SYNC_POSIX_ASSERT(pthread_cond_wait(&cv, &mtx), "pthread_cond_wait failed");
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
        SYNC_POSIX_ASSERT(pthead_cond_timedwait(&cv, &mtx, &ts), "pthread_cond_timedwait failed");
    }

    void signal_cv(sync_cv_t& cv) {
        SYNC_POSIX_ASSERT(pthread_cond_signal(&cv), "pthread_cond_signal failed");
    }

    void broadcast_cv(sync_cv_t& cv) {
        SYNC_POSIX_ASSERT(pthread_cond_broadcast(&cv), "pthread_cond_broadcast failed");
    }

#endif

}
