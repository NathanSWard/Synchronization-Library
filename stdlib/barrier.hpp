// barrier.hpp
#pragma once

#include "include/assert.hpp"
#include "condition_variable.hpp"
#include "mutex.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <type_traits>

namespace sync {

class barrier {
public:
    explicit barrier(std::ptrdiff_t num_threads) 
        : count_{num_threads}
        , threads_{num_threads}
    {}

    barrier(barrier const&) = delete;
    barrier& operator=(barrier const&) = delete;

#ifndef DISABLE_SYNC_ASSERT
    ~barrier() {
        scoped_lock lock{mtx_};
        SYNC_ASSERT(count_ == threads_, "Threads are still waiting on a barrier");
    }
#endif

    void arrive_and_wait() {
        unique_lock lock{mtx_};
        if (--count_ == 0) {
            count_ = threads_;
            lock.unlock();
            cv_.notify_all();
        }
        else
            cv_.wait(lock, [this]{return count_ == threads_;});
    }

    // does not block
    void arrive_and_drop() {
        unique_lock lock{mtx_};
        if (--count_ == 0) {
            count_ = --threads_;
            lock.unlock();
            cv_.notify_all();
        }
        else
            --threads_;
    }

private:
    condition_variable  cv_;
    mutex               mtx_;
    std::ptrdiff_t      count_;
    std::ptrdiff_t      threads_;
};

class flex_barrier {
public:
    explicit flex_barrier(std::ptrdiff_t num_threads) 
        : compl_{[]()->std::ptrdiff_t{return -1;}}
        , count_{num_threads}
        , threads_{num_threads}
    {}

    template<class Fn>
    flex_barrier(std::ptrdiff_t num_threads, Fn completion) 
        : compl_{completion}
        , count_{num_threads}
        , threads_{num_threads}
    {
        static_assert(std::is_copy_constructible_v<Fn>, 
            "flex_barrier's completion function must be copy constructible");
        static_assert(std::is_nothrow_invocable_r_v<std::ptrdiff_t, Fn>, 
            "flex_barrier's completion function must satisfy the following contstraint : std::is_nothrow_invocable_r<std::ptrdiff_t, Fn>");
    }

    flex_barrier(flex_barrier const&) = delete;
    flex_barrier& operator=(flex_barrier const&) = delete;

#ifndef DISABLE_SYNC_ASSERT
    ~flex_barrier() {
        scoped_lock lock{mtx_};
        SYNC_ASSERT(count_ == threads_, "Threads are still waiting on a flex_barrier");
    }
#endif

    void arrive_and_wait() {
        unique_lock lock{mtx_};
        if (--count_ == 0) {
            if (std::ptrdiff_t const val = compl_(); val != -1)
                threads_ = val;
            lock.unlock();
            cv_.notify_all();
        }
        else
            cv_.wait(lock, [this]{return count_ == threads_;});
    }

    // does not block
    void arrive_and_drop() {
        unique_lock lock{mtx_};
        if (--count_ == 0) {
            if (std::ptrdiff_t val = compl_(); val != -1)
                threads_ = val;
            lock.unlock();
            cv_.notify_all();
        }
        else
            --threads_;
    }

private:
    std::function<std::ptrdiff_t()> compl_;
    condition_variable              cv_;
    mutex                           mtx_;
    std::ptrdiff_t                  count_;
    std::ptrdiff_t                  threads_;
};

}
