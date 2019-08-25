#pragma once

#include "platform.hpp"

#if SYNC_WINDOWS

#include <syncapi.h>

// mutex types
using sync_mtx_t = CRITICAL_SECTION;
using sync_rw_mtx_t = SRWLock;

// condition variable
using sync_cv_t = CONDITION_VARIABLE;

#elif SYNC_MAC || SYNC_LINUX

#include <pthread.h>

// mutex types
using sync_mtx_t = pthread_mutex_t;
using sync_rw_mtx_t = pthread_rwlock_t;

// condition variable
using sync_cv_t = pthread_cond_t;

#endif