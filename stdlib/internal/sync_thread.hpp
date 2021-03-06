// sync_thread.hpp
#pragma once

#include "include/assert.hpp"
#include "include/platform.hpp"
#include "include/types.hpp"

#include <chrono>

#if SYNC_MAC || SYNC_LINUX
    #include <errno.h>
#endif

namespace sync {

void sync_thread_create(sync_thread_t&, void*(*)(void*), void*);
void sync_thread_join(sync_thread_t&);
void sync_thread_detach(sync_thread_t&);
void sync_thread_yield();
sync_thread_id_t sync_thread_curr_id();
sync_thread_id_t sync_thread_id(sync_thread_t const&);
bool sync_thread_id_equal(sync_thread_id_t, sync_thread_id_t);
void sync_thread_sleep_for(std::chrono::nanoseconds const&);
bool sync_thread_is_null(sync_thread_t const&);
static unsigned int sync_thread_getconcurrency() noexcept;

#if SYNC_WINDOWS

void sync_thread_create(sync_thread_t& t, void*(*f)(void*), void* args) {
    t = CreateThread(nullptr, 0, f, args, 0);
}

void sync_thread_join(sync_thread_t& t) {
    SYNC_ASSERT(WaitForSingleObject(t, INFINITE) != WAIT_FAILED, "WaitForSingleObject failed");
    SYNC_WINDOWS_ASSERT(CloseHandle(t), "CloseHandle for joined thread failed");
}

void sync_thread_detach(sync_thread_t&) {
    SYNC_WINDOWS_ASSERT(CloseHandle(t), "CloseHandle for detached thread failed");
}

void sync_thread_yield() {
    (void)SwitchToThread();
}
sync_thread_id_t sync_thread_curr_id() {
    return GetCurrentThreadId();
}
sync_thread_id_t sync_thread_id(sync_thread_t const& t) {
    return GetThreadId(t);
}

bool sync_thread_id_equal(sync_thread_id_t a, sync_thread_id_t b) {
    return a == b;
}

void sync_thread_sleep_for(std::chrono::nanoseconds const& ns) {
    Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(ns).count()));
}

bool sync_thread_is_null(sync_thread_t const& t) {
    return t == INVALID_HANDLE_VALUE;
}

static unsigned int sync_thread_getconcurrency() noexcept {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return static_cast<unsigned int>(sysinfo.dwNumberOfProcessors);
}

#elif SYNC_MAC || SYNC_LINUX

void sync_thread_create(sync_thread_t& t, void*(*f)(void*), void* args) {
    SYNC_POSIX_ASSERT(pthread_create(&t, nullptr, f, args), "pthread_create failed");
}

void sync_thread_join(sync_thread_t& t) {
    SYNC_POSIX_ASSERT(pthread_join(t, nullptr), "pthread_join failed");
    t = SYNC_NULL_THREAD;
}

void sync_thread_detach(sync_thread_t& t) {
    SYNC_POSIX_ASSERT(pthread_detach(t), "pthread_detach failed");
}

void sync_thread_yield() {
    SYNC_POSIX_ASSERT(sched_yield(), "sched_yield failed");
}

sync_thread_id_t sync_thread_curr_id() {
    return pthread_self();
}

sync_thread_id_t sync_thread_id(sync_thread_t const& t) {
    return t;
}

bool sync_thread_id_equal(sync_thread_id_t t1, sync_thread_id_t t2) {
    return pthread_equal(t1, t2);
}

void sync_thread_sleep_for(std::chrono::nanoseconds const& ns) {
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

bool sync_thread_is_null(sync_thread_t const& t) {
    return t == 0;
}

static unsigned int sync_thread_getconcurrency() noexcept {
    return static_cast<unsigned int>(pthread_getconcurrency());
}

#endif

}
