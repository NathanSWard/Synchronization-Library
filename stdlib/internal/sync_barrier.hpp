// sync_barrier.hpp
#pragma once

#include "include/assert.hpp"
#include "include/types.hpp"

namespace sync {

void sync_barrier_init(sync_barrier_t&, unsigned int);
void sync_barrier_destroy(sync_barrier_t&);
bool sync_barrier_wait(sync_barrier_t&);

#if SYNC_WINDOWS

void sync_barrier_init(sync_barrier_t& bar, unsigned int count) {
    SYNC_ASSERT(InitializeSynchronizationBarrier(&bar, count, -1) == true, 
        "InitializeSynchronizationBarrier failed.");
}

void sync_barrier_destroy(sync_barrier_t& bar) {
    (void)DeleteSynchronizationBarrier(&bar);
}

bool sync_barrier_wait(sync_barrier_t&) {
    return EnterSynchronizationBarrier(&bar, SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE);
}

#elif SYNC_MAC || SYNC_LINUX

void sync_barrier_init(sync_barrier_t& bar, unsigned int count) {
    pthread_barrier_init(&bar, nullptr, count);
}

void sync_barrier_destroy(sync_barrier_t& bar) {
    pthread_barrier_destroy(&bar);
}

// returns true for the last thread
bool sync_barrier_wait(sync_barrier_t& bar) {
    return pthread_barrier_wait(&bar);
}

#endif

} // namespace sync