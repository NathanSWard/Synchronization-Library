#pragma once

#include <chrono>
#include "platform.hpp"
#include "types.hpp"

#if SYNC_MAC || SYNC_LINUX
    #include <errno.h>
#endif

namespace sync {

void initialize_thread(sync_thread_t&, void*(*)(void*), void*);
void join_thread(sync_thread_t&);
void detach_thread(sync_thread_t&);
void yeild_thread();
sync_thread_id_t get_curr_thread_id();
sync_thread_id_t get_thread_id(sync_thread_t const&);
bool thread_id_equal(sync_thread_id_t, sync_thread_id_t);
void thread_sleep_for(std::chrono::nanoseconds const&);
bool is_thread_null(sync_thread_t const&);
static unsigned int get_concurrency() noexcept;

#if SYNC_WINDOWS

void initialize_thread(sync_thread_t& t, void*(*f)(void*), void* args) {
    t = CreateThread(nullptr, 0, f, args, 0);
}

void join_thread(sync_thread_t& t) {
    (void)WaitForSingleObject(t, INFINITE);
    (void)CloseHandle(t);
}

void detach_thread(sync_thread_t&) {
    (void)CloseHandle(t);
}

void yeild_thread() {
    (void)SwitchToThread();
}
sync_thread_id_t get_curr_thread_id() {
    return GetCurrentThreadId();
}
sync_thread_id_t get_thread_id(sync_thread_t const& t) {
    return GetThreadId(t);
}

bool thread_id_equal(sync_thread_id_t a, sync_thread_id_t b) {
    return a == b;
}

void thread_sleep_for(std::chrono::nanoseconds const& ns) {
    Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(ns).count()));
}

bool is_thread_null(sync_thread_t const& t) {
    return t == INVALID_HANDLE_VALUE;
}

static unsigned int get_concurrency() noexcept {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return static_cast<unsigned int>(sysinfo.dwNumberOfProcessors);
}

#elif SYNC_MAC || SYNC_LINUX

void initialize_thread(sync_thread_t& t, void*(*f)(void*), void* args) {
    (void)pthread_create(&t, nullptr, f, args);
}

void join_thread(sync_thread_t& t) {
    (void)pthread_join(t, nullptr);
    t = SYNC_NULL_THREAD;
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

sync_thread_id_t get_thread_id(sync_thread_t const& t) {
    return t;
}

bool thread_id_equal(sync_thread_id_t t1, sync_thread_id_t t2) {
    return pthread_equal(t1, t2);
}

void thread_sleep_for(std::chrono::nanoseconds const& ns) {
   std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ns);
   ::timespec ts;
   using ts_sec = decltype(ts.tv_sec);
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

bool is_thread_null(sync_thread_t const& t) {
    return t == 0;
}

static unsigned int get_concurrency() noexcept {
    return static_cast<unsigned int>(pthread_getconcurrency());
}

#endif

}
