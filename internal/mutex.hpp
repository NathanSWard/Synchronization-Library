#pragma once

#include "platform.hpp"

namespace sync {
    struct sync_mutex;

    void initialize_mutex(sync_mutex&);
    void destroy_mutex(sync_mutex&);    
    void acquire_mutex(sync_mutex&);
    bool try_acquire_mutex(sync_mutex&);
    void release_mutex(sync_mutex&);

    struct sync_rw_mutex;

    void initialize_mutex(sync_rw_mutex&);
    void destroy_mutex(sync_rw_mutex&);
    void acquire_mutex_wr(sync_rw_mutex&);
    void acquire_mutex_rd(sync_rw_mutex&);
    bool try_acquire_mutex_wr(sync_rw_mutex&);
    bool try_acquire_mutex_rd(sync_rw_mutex&);
    void release_mutex_wr(sync_rw_mutex&);
    void release_mutex_rd(sync_rw_mutex&);
}

#if SYNC_WINDOWS
    #include "mutex_win.hpp"
#elif SYNC_MAX || SYNC_LINUX 
    #include "mutex_posix.hpp"
#endif