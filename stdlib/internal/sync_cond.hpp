// sync_cond.hpp
#pragma once

#include "include/assert.hpp"
#include "include/types.hpp"

#include <chrono>

namespace sync {

void sync_cond_init(sync_cond_t&);
void sync_cond_destroy(sync_cond_t&);
template<class Mutex>
void sync_cond_wait(sync_cond_t&, Mutex&);
template<class Mutex>
void sync_cond_timedwait(sync_cond_t&, Mutex&, 
                    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>);
void sync_cond_signal(sync_cond_t&);
void sync_cond_broadcast(sync_cond_t&);

#if SYNC_WINDOWS

    void sync_cond_init(sync_cond_t& cv) {
        InitializeConditionVariable(cv);
    }

    void sync_cond_destroy(sync_cond_t&) {}

    template<class Mutex>
    void sync_cond_wait(sync_cond_t& cv, Mutex& mtx) {
        if constexpr (std::is_same_v<Mutex, CRITICAL_SECTION>) {
            SYNC_WINDOWS_ASSERT(SleepConditionVariableCS(&cv, &mtx, INFINITE), "SleepConditionVariableCS failed");
        } else if (std::is_same_v<Mutex, SRWLock>) {
            SYNC_WINDOWS_ASSERT(SleepConditionVariableSRW(&cv, &mtx, INFINITE, 0), "SleepConditionVariableSRW failed");
        } else {
            static_assert(false, "Windows sync_cond_wait only takes a SRWLock or Cricial Section");
        }
    }

    template<class Mutex>
    void sync_cond_timedwait(sync_cond_t& cv, Mutex& mtx, 
                        std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp) {
        auto duration{std::chrono::duration_cast<milliseconds>(tp.time_since_epoch()).count()};
        if constexpr (std::is_same_v<Mutex, CRITICAL_SECTION>) {
            SYNC_WINDOWS_ASSERT(SleepConditionVariableCS(&cv, &mtx, static_cast<DWORD>(duration)), "SleepConditionVariableCS failed");
        } else if (std::is_same_v<Mutex, SRWLock>) {
            SYNC_WINDOWS_ASSERT(SleepConditionVariableSRW(&cv, &mtx, static_cast<DWORD>(duration), 0), "SleepConditionVariableSRW failed");
        } else {
            static_assert(false, "Windows sync_cond_timedwait only takes a SRWLock or Cricial Section");
        }
    }

    void sync_cond_signal(sync_cond_t& cv) {
        WakeConditionVariable(&cv);
    }

    void sync_cond_broadcast(sync_cond_t& cv) {
        WakeAllConditionVariable(&cv);
    }

#elif SYNC_MAX || SYNC_LINUX

    void sync_cond_init(sync_cond_t& cv) {
        SYNC_POSIX_ASSERT(pthread_cond_init(&cv, nullptr), "pthread_cond_init failed");
    }

    void sync_cond_destroy(sync_cond_t& cv) {
        SYNC_POSIX_ASSERT(pthread_cond_destroy(&cv), "pthread_cond_destroy failed");
    }

    template<class Mutex>
    void sync_cond_wait(sync_cond_t& cv, Mutex& mtx) {
        SYNC_POSIX_ASSERT(pthread_cond_wait(&cv, &mtx), "pthread_cond_wait failed");
    }

    template<class Mutex>
    void sync_cond_timedwait(sync_cond_t& cv, Mutex& mtx, 
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
        (void)pthread_cond_timedwait(&cv, &mtx, &ts);
    }

    void sync_cond_signal(sync_cond_t& cv) {
        SYNC_POSIX_ASSERT(pthread_cond_signal(&cv), "pthread_cond_signal failed");
    }

    void sync_cond_broadcast(sync_cond_t& cv) {
        SYNC_POSIX_ASSERT(pthread_cond_broadcast(&cv), "pthread_cond_broadcast failed");
    }

#endif

} // namespace sync
