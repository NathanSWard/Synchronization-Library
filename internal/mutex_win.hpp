#pragma once

#include <synchapi.h>

namespace sync {

    // basic mutex
    struct sync_mutex {
        CRITICAL_SECTION handle_;
    };

    void initialize_mutex(sync_mutex& mtx) {
        InitializeCriticalSection(&mtx.handle_);
    }
    
    void acquire_mutex(sync_mutex& mtx) {
        EnterCriticalSection(&mtx.handle_);
    }

    bool try_acquire_mutex(sync_mutex& mtx) {
        return TryEnterCriticalSection(&mtx.handle_);
    }

    void release_mutex(sync_mutex& mtx) {
        LeaveCriticalSection(&mtx.handle_);
    }
    
    void destroy_mutex(sync_mutex& mtx) {
        DeleteCriticalSection(&mtx.handle_);
    }

    // reader writer mutex
    struct sync_rw_mutex {
        SRWLock handle_;
    }

    void initialize_mutex(sync_rw_mutex& mtx) {
        InitializeSRWLock(&mtx.handle_)
    }

    void destroy_mutex(sync_rw_mutex&) {}

    void acquire_mutex_wr(sync_rw_mutex& mtx) {
        AcquireSRWLockExclusive(&mtx.handle_);
    }

    void acquire_mutex_rd(sync_rw_mutex& mtx) {
        AcquireSRWLockShared(&mtx.handle_);
    }

    bool try_acquire_mutex_wr(sync_rw_mutex& mtx) {
        TryAcquireSRWLockExclusive(&mtx.handle_);
    }

    bool try_acquire_mutex_rd(sync_rw_mutex& mtx) {
        TryAcquireSRWLockShared(&mtx.handle_);
    }

    void release_mutex_wr(sync_rw_mutex& mtx) {
        ReleaseSRWLockExclusive(&mtx.handle_);
    }

    void release_mutex_rd(sync_rw_mutex& mtx) {
        ReleaseSRWLockShared(&mtx.handle_);
    }

}
