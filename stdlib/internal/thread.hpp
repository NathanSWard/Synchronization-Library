#pragma once

#include <chrono>
#include "platform.hpp"
#include "types.hpp"

namespace sync {

void initialize_thread(sync_thread_t&, void*(*)(void*), void*);
void join_thread(sync_thread_t&);
void detach_thread(sync_thread_t&);
void yeild_thread();
sync_thread_id_t get_curr_thread_id();
sync_thread_id_t get_thread_id(sync_thread_t const&);
bool thread_id_equal(sync_thread_id_t, sync_thread_id_t);
void thread_sleep_for(std::chrono::nanoseconds const&);
bool is_thread_null(sync_thread_t&);
int get_concurrency();

}

#if SYNC_WINDOWS

#elif SYNC_MAC || SYNC_LINUX

#include <errno.h>

void initialize_thread(sync_thread_t& t, void*(*f)(void*), void* args) {
    (void)pthread_create(&t, nullptr, f, args);
}

void join_thread(sync_thread_t& t) {
    (void)pthread_join(t, 0);
}

void detach_thread(sync_thread_t& t) {
    (void)pthread_detach(t);
}

void yeild_thread() {
    sched_yield();
}

sync_thread_id_t get_curr_thread_id() {
    return pthread_self();
}

sync_thread_id_t get_thread_id(sync_thread_t t) {
    return t;
}

bool thread_id_equal(sync_thread_id_t t1, sync_thread_id_t t2) {
    return pthread_equal(t1, t2);
}

void thread_sleep_for(std::chrono::nanoseconds const& ns) {
   std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ns);
   ::timespec ts;
   typedef decltype(ts.tv_sec) ts_sec;
   constexpr ts_sec ts_sec_max = std::numeric_limits<ts_sec>::max();

   if (s.count() < ts_sec_max) {
     ts.tv_sec = static_cast<ts_sec>(s.count());
     ts.tv_nsec = static_cast<decltype(ts.tv_nsec)>((ns - s).count());
   }
   else {
     ts.tv_sec = ts_sec_max;
     ts.tv_nsec = 999999999; // (10^9 - 1)
   }

   while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
}

bool is_thread_null(sync_thread_t& t) {
    return t == 0;
}

unsigned int get_concurrency() {
    return static_cast<unsigned int>(pthread_getconcurrency());
}

#endif