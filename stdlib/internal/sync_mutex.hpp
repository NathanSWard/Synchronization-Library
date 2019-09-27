// sync_mutex.hpp
#pragma once

#include "include/assert.hpp"
#include "include/types.hpp"

namespace sync {

    void sync_mutex_init(sync_mutex_t&);
    void sync_mutex_destroy(sync_mutex_t&);    
    void sync_mutex_lock(sync_mutex_t&);
    bool sync_mutex_trylock(sync_mutex_t&);
    void sync_mutex_unlock(sync_mutex_t&);

    void sync_rwlock_init(sync_rwlock_t&);
    void sync_rwlock_destroy(sync_rwlock_t&);
    void sync_rwlock_wrlock(sync_rwlock_t&);
    void sync_rwlock_rdlock(sync_rwlock_t&);
    bool sync_rwlock_trywrlock(sync_rwlock_t&);
    bool sync_rwlock_tryrdlock(sync_rwlock_t&);
    void sync_rwlock_wrunlock(sync_rwlock_t&);
    void sync_rwlock_rdunlock(sync_rwlock_t&);

#if SYNC_WINDOWS

    // basic mutex
    void sync_mutex_init(sync_mutex_t& mtx) {
        InitializeCriticalSection(&mtx);
    }
    
    void sync_mutex_lock(sync_mutex_t& mtx) {
        EnterCriticalSection(&mtx);
    }

    bool sync_mutex_trylock(sync_mutex_t& mtx) {
        return TryEnterCriticalSection(&mtx);
    }

    void sync_mutex_unlock(sync_mutex_t& mtx) {
        LeaveCriticalSection(&mtx);
    }
    
    void sync_mutex_destroy(sync_mutex_t& mtx) {
        DeleteCriticalSection(&mtx);
    }

    // reader writer mutex
    void sync_rwlock_init(sync_rwlock_t& mtx) {
        InitializeSRWLock(&mtx)
    }

    void sync_rwlock_destroy(sync_rwlock_t&) {}

    void sync_rwlock_wrlock(sync_rwlock_t& mtx) {
        AcquireSRWLockExclusive(&mtx);
    }

    void sync_rwlock_rdlock(sync_rwlock_t& mtx) {
        AcquireSRWLockShared(&mtx);
    }

    bool sync_rwlock_trywrlock(sync_rwlock_t& mtx) {
        return TryAcquireSRWLockExclusive(&mtx);
    }

    bool sync_rwlock_tryrdlock(sync_rwlock_t& mtx) {
        return TryAcquireSRWLockShared(&mtx);
    }

    void sync_rwlock_wrunlock(sync_rwlock_t& mtx) {
        ReleaseSRWLockExclusive(&mtx);
    }

    void sync_rwlock_rdunlock(sync_rwlock_t& mtx) {
        ReleaseSRWLockShared(&mtx);
    }

#elif SYNC_MAX || SYNC_LINUX 
    
    // basic mutex
    void sync_mutex_init(sync_mutex_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_init(&mtx, nullptr), "pthread_mutex_init failed");
    }
    
    void sync_mutex_lock(sync_mutex_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_lock(&mtx), "pthread_mutex_lock failed");
    }

    bool sync_mutex_trylock(sync_mutex_t& mtx) {
        return (pthread_mutex_trylock(&mtx) == 0);
    }

    void sync_mutex_unlock(sync_mutex_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_unlock(&mtx), "pthread_mutex_unlock failed");
    }
    
    void sync_mutex_destroy(sync_mutex_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_mutex_destroy(&mtx), "pthread_mutex_destroy failed");
    }

    // reader writer mutex
    void sync_rwlock_init(sync_rwlock_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_init(&mtx, nullptr), "pthread_rwlock_init failed");
    }

    void sync_rwlock_destroy(sync_rwlock_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_destroy(&mtx), "pthread_rwlock_destroy failed");
    }

    void sync_rwlock_wrlock(sync_rwlock_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_wrlock(&mtx), "pthread_rwlock_wrlock failed");
    }

    void sync_rwlock_rdlock(sync_rwlock_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_rdlock(&mtx), "pthread_rwlock_rdlock failed");
    }

    bool sync_rwlock_trywrlock(sync_rwlock_t& mtx) {
        return (pthread_rwlock_trywrlock(&mtx) == 0);
    }

    bool sync_rwlock_tryrdlock(sync_rwlock_t& mtx) {
        return (pthread_rwlock_tryrdlock(&mtx) == 0);
    }

    void sync_rwlock_wrunlock(sync_rwlock_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_unlock(&mtx), "pthread_rwlock_unlock failed");
    }

    void sync_rwlock_rdunlock(sync_rwlock_t& mtx) {
        SYNC_POSIX_ASSERT(pthread_rwlock_unlock(&mtx), "pthread_rwlock_unlock failed");
    }

#endif

} // namespace sync