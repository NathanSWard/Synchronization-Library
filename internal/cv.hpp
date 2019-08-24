#pragma once

#include "mutex.hpp"
#include "platform.hpp"

#include <chrono>

namespace sync {

struct sync_condition_variable;

void initialize_cv(sync_condition_variable&);
void destroy_cv(sync_condition_variable&);
template<class Mutex>
void wait_cv(sync_condition_variable&, Mutex&);
template<class Mutex, class Duration>
void timed_wait_cv(sync_condition_variable&, Mutex&, 
                    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>);
void signal_cv(sync_condition_variable&);
void broadcast_cv(sync_condition_variable&);

}

#if SYNC_WINDOWS
    #include "cv_win.hpp"
#elif SYNC_MAX || SYNC_LINUX 
    #include "cv_posix.hpp"
#endif