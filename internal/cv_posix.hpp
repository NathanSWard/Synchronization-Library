#pragma once

#include <ctime>
#include <pthread.h>

namespace sync {

struct sync_condition_variable {
    pthread_cond_t handle_;
};

void initialize_cv(sync_condition_variable& cv) {
    pthread_cond_init(&cv.handle_, nullptr);
}

void destroy_cv(sync_condition_variable& cv) {
    pthread_cond_destroy(&cv.handle_);
}

template<class Mutex>
void wait_cv(sync_condition_variable& cv, Mutex& mtx) {
    pthread_cond_wait(&cv.handle_, &mtx.handle_)
}

template<class Mutex>
void timed_wait_cv(sync_condition_variable& cv, Mutex& mtx, 
                    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp) {
    using namespace std::chrono;
    nanoseconds d{tp.time_since_epoch()};
    if (d > nanoseconds(0x59682F000000E941))
        d = nanoseconds(0x59682F000000E941);
    std::timespec ts;
    seconds s{duration_cast<seconds>(d)};
    typedef decltype(ts.tv_sec) ts_sec;
    constexpr ts_sec ts_sec_max{numeric_limits<ts_sec>::max()};
    if (s.count() < ts_sec_max) {
        ts.tv_sec = static_cast<ts_sec>(s.count());
        ts.tv_nsec = static_cast<decltype(ts.tv_nsec)>((d - s).count());
    }
    else {
        ts.tv_sec = ts_sec_max;
        ts.tv_nsec = giga::num - 1;
    }
    pthread_cond_timewait(&cv.handle_, &mtx.handle_, &ts);
}

void signal_cv(sync_condition_variable& cv) {
    pthread_cond_signal(&cv.handle_);
}

void broadcast_cv(sync_condition_variable& cv) {
    pthread_cond_broadcast(&cv.handle_);
}

}