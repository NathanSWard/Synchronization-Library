// barrier_extra.hpp
#pragma once

#include "../stdlib/internal/sync_barrier.hpp"

#include <functional>

namespace sync { namespace os {

class barrier {
public:
    barrier(unsigned int count) {
        sync_barrier_init(bar_, count);
    }

    ~barrier() {
        sync_barrier_destroy(bar_);
    }

    void wait() {
        sync_barrier_wait(bar_);
    }

private:
    sync_barrier_t bar_;
};

class flex_barrier {
public:
    template<class Fn>
    flex_barrier(unsigned int count, Fn completion) 
        : compl_{completion}
    {
        sync_barrier_init(bar_, count);
    }

    ~flex_barrier() {
        sync_barrier_destroy(bar_);
    }

    void wait() {
        if (sync_barrier_wait(bar_))
            compl_();
    }

private:
    std::function<void()>   compl_;
    sync_barrier_t          bar_;
};

} // namespace os

} // namespace sync