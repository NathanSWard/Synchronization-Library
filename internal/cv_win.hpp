#pragma once

#include <chrono>
#include <synchapi.h>
#include <type_traits>

namespace sync {

struct sync_condition_variable {
    CONDITION_VARIALBE handle_;
};

void initialize_cv(sync_condition_variable& cv) {
    InitializeConditionVariable(cv.handle_);
}

void destroy_cv(sync_condition_variable&) {}

template<class Mutex>
void wait_cv(sync_condition_variable& cv, Mutex& mtx) {
    if constexpr (std::is_same_v<Mutex, CRITICAL_SECTION>) {
        SleepConditionVariableCS(&cv.handle_, &mtx.handle_, INFINITE);
    } else if (std::is_same_v<Mutex, SRWLock>) {
        SleepConditionVariableSRW(&cv.handle_, &mtx.handle_, INFINITE, 0);
    } else {
        static_assert(false, "Windows wait_cv only takes a SRWLock or Cricial Section");
    }
}

template<class Mutex, class TimePoint>
void timed_wait_cv(sync_condition_variable& cv, Mutex& mtx, 
                    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp) {
    auto duration{std::chrono::duration_cast<milliseconds>(tp.time_since_epoch()).count()};
    if constexpr (std::is_same_v<Mutex, CRITICAL_SECTION>) {
        SleepConditionVariableCS(&cv.handle_, &mtx.handle_, static_cast<DWORD>(duration));
    } else if (std::is_same_v<Mutex, SRWLock>) {
        SleepConditionVariableSRW(&cv.handle_, &mtx.handle_, static_cast<DWORD>(duration), 0);
    } else {
        static_assert(false, "Windows timed_wait_cv only takes a SRWLock or Cricial Section");
    }
}

void signal_cv(sync_condition_variable& cv) {
    WakeConditionVariable(&cv.handle_);
}

void broadcast_cv(sync_condition_variable& cv) {
    WakeAllConditionVariable(&cv.handle_);
}

}