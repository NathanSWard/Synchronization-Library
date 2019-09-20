#pragma once

#include "platform.hpp"

#if SYNC_WINDOWS

#include <processthreadapi.h>
#include <syncapi.h>

// thread type
using sync_thread_t = HANDLE;
using sync_thread_id_t = DWORD;
#define SYNC_NULL_THREAD nullptr

// mutex types
using sync_mtx_t = CRITICAL_SECTION;
using sync_rw_mtx_t = SRWLock;

#define SYNC_RECURSIVE_MTX_INIT {}

// condition variable
using sync_cv_t = CONDITION_VARIABLE;

#elif SYNC_MAC || SYNC_LINUX

#include <pthread.h>

// thread types
using sync_thread_t = pthread_t;
using sync_thread_id_t = pthread_t;
#define SYNC_NULL_THREAD 0

// mutex types
using sync_mtx_t = pthread_mutex_t;
using sync_rw_mtx_t = pthread_rwlock_t;

#if SYNC_MAC
    #define SYNC_RECURSIVE_MTX_INIT PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#elif SYNC_LINUX
    #define SYNC_RECURSIVE_MTX_INIT PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

// condition variable
using sync_cv_t = pthread_cond_t;

#endif
