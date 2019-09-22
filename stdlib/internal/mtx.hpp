#pragma once

#include "include/assert.hpp"
#include "include/types.hpp"

namespace sync {

    void initialize_mutex(sync_mtx_t&);
    void destroy_mutex(sync_mtx_t&);    
    void acquire_mutex(sync_mtx_t&);
    bool try_acquire_mutex(sync_mtx_t&);
    void release_mutex(sync_mtx_t&);

    void initialize_mutex(sync_rw_mtx_t&);
    void destroy_mutex(sync_rw_mtx_t&);
    void acquire_mutex_wr(sync_rw_mtx_t&);
    void acquire_mutex_rd(sync_rw_mtx_t&);
    bool try_acquire_mutex_wr(sync_rw_mtx_t&);
    bool try_acquire_mutex_rd(sync_rw_mtx_t&);
    void release_mutex_wr(sync_rw_mtx_t&);
    void release_mutex_rd(sync_rw_mtx_t&);

#if SYNC_WINDOWS

    // basic mutex
    void initialize_mutex(sync_mtx_t& mtx) {
        InitializeCriticalSection(&mtx);
    }
    
    void acquire_mutex(sync_mtx_t& mtx) {
        EnterCriticalSection(&mtx);
    }

    bool try_acquire_mutex(sync_mtx_t& mtx) {
        return TryEnterCriticalSection(&mtx);
    }

    void release_mutex(sync_mtx_t& mtx) {
        LeaveCriticalSection(&mtx);
    }
    
    void destroy_mutex(sync_mtx_t& mtx) {
        DeleteCriticalSection(&mtx);
    }

    // reader writer mutex
    void initialize_mutex(sync_rw_mtx_t& mtx) {
        InitializeSRWLock(&mtx)
    }

    void destroy_mutex(sync_rw_mtx_t&) {}

    void acquire_mutex_wr(sync_rw_mtx_t& mtx) {
        AcquireSRWLockExclusive(&mtx);
    }

    void acquire_mutex_rd(sync_rw_mtx_t& mtx) {
        AcquireSRWLockShared(&mtx);
    }

    bool try_acquire_mutex_wr(sync_rw_mtx_t& mtx) {
        return TryAcquireSRWLockExclusive(&mtx);
    }

    bool try_acquire_mutex_rd(sync_rw_mtx_t& mtx) {
        return TryAcquireSRWLockShared(&mtx);
    }

    void release_mutex_wr(sync_rw_mtx_t& mtx) {
        ReleaseSRWLockExclusive(&mtx);
    }

    void release_mutex_rd(sync_rw_mtx_t& mtx) {
        ReleaseSRWLockShared(&mtx);
    }

#elif SYNC_MAX || SYNC_LINUX 
    
    // basic mutex
    void initialize_mutex(sync_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_init(&mtx, nullptr), "pthread_mutex_init failed");
    }
    
    void acquire_mutex(sync_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_lock(&mtx), "pthread_mutex_lock failed");
    }

    bool try_acquire_mutex(sync_mtx_t& mtx) {
        return (pthread_mutex_trylock(&mtx) == 0);
    }

    void release_mutex(sync_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_unlock(&mtx), "pthread_mutex_unlock failed");
    }
    
    void destroy_mutex(sync_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_destroy(&mtx), "pthread_mutex_destroy failed");
    }

    // reader writer mutex
    void initialize_mutex(sync_rw_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_init(&mtx, nullptr), "pthread_rwlock_init failed");
    }

    void destroy_mutex(sync_rw_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_destroy(&mtx), "pthread_rwlock_destroy failed");
    }

    void acquire_mutex_wr(sync_rw_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_wrlock(&mtx), "pthread_rwlock_wrlock failed");
    }

    void acquire_mutex_rd(sync_rw_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_rdlock(&mtx), "pthread_rwlock_rdlock failed");
    }

    bool try_acquire_mutex_wr(sync_rw_mtx_t& mtx) {
        return (pthread_rwlock_trywrlock(&mtx) == 0);
    }

    bool try_acquire_mutex_rd(sync_rw_mtx_t& mtx) {
        return (pthread_rwlock_tryrdlock(&mtx) == 0);
    }

    void release_mutex_wr(sync_rw_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_unlock(&mtx), "pthread_rwlock_unlock failed");
    }

    void release_mutex_rd(sync_rw_mtx_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_unlock(&mtx), "pthread_rwlock_unlock failed");
    }

#endif

}