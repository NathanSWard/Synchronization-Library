#include <pthread.h>

namespace sync {

    // basic mutex
    struct sync_mutex {
        pthread_mutex_t handle_;
    };

    void initialize_mutex(sync_mutex& mtx) {
        pthread_mutex_init(&mtx.handle_, nullptr);
    }
    
    void acquire_mutex(sync_mutex& mtx) {
        pthread_mutex_lock(&mtx.handle_);
    }

    bool try_acquire_mutex(sync_mutex& mtx) {
        return (pthread_mutex_trylock(&mtx.handle_) == 0);
    }

    void release_mutex(sync_mutex& mtx) {
        pthread_mutex_unlock(&mtx.handle_);
    }
    
    void destroy_mutex(sync_mutex& mtx) {
        pthread_mutex_destroy(&mtx.handle_);
    }

    // reader writer mutex
    struct sync_rw_mutex {
        pthread_rwlock_t handle_;
    };

    void initialize_mutex(sync_rw_mutex& mtx) {
        pthread_rwlock_init(&mtx.handle_, nullptr);
    }

    void destroy_mutex(sync_rw_mutex& mtx) {
        pthread_rwlock_destroy(&mtx.handle_);
    }

    void acquire_mutex_wr(sync_rw_mutex& mtx) {
        pthread_rwlock_wrlock(&mtx.handle_);
    }

    void acquire_mutex_rd(sync_rw_mutex& mtx) {
        pthread_rwlock_rdlock(&mtx.handle_);
    }

    bool try_acquire_mutex_wr(sync_rw_mutex& mtx) {
        pthread_rwlock_trywrlock(&mtx.handle_);
    }

    bool try_acquire_mutex_rd(sync_rw_mutex& mtx) {
        pthread_rwlock_tryrdlock(&mtx.handle_);
    }

    void release_mutex_wr(sync_rw_mutex& mtx) {
        pthread_rwlock_unlock(&mtx.handle_);
    }

    void release_mutex_rd(sync_rw_mutex& mtx) {
        pthread_rwlock_unlock(&mtx.handle_);
    }

}
