#pragma once

#include "internal/thread.hpp"

#include <exception>
#include <utility>

namespace sync {

class thread {
public:
    using native_handle_type = sync_thread_t;
    using id = sync_thread_id_t;

    // Member functions
    thread() noexcept = default;

    thread(thread&& other) noexcept 
        : handle_{other.handle_}
    {
        other.handle_ = SYNC_NULL_THREAD;
    }

    thread& operator=(thread&& other) noexcept {
        if (joinable())
            std::terminate();
        handle_ = std::exchange(other.handle_, SYNC_NULL_THREAD);
    }

    template <class Function, class ...Args>
    explicit thread(Function&& f, Args&&... args) {
        /*
        c++ 20 perfect capture
        initialize_thread(handle_, 
            [f = std::forward<Function>(f), ...args = std::forward<Args>(args)](void*) -> void* {
                f(args...); 
                return nullptr;
            },
            nullptr);
        */
        initialize_thread(handle_,
            [f = std::forward<Function>(f), tup = std::forward_as_tuple(std::forward<Args>(args)...)]
            (void*) -> void* {
                std::apply([&](auto&&... args){
                    (void)f(args...);
                }, tup);
                return nullptr;
            },
            nullptr);
    }

    ~thread() {
        if (joinable())
            std::terminate();
    }

    // Observers
    bool joinable() {
        return is_thread_null(handle_);
    }

    id get_id() const noexcept {
        return get_thread_id(handle_);
    }

    native_handle_type native_handle() {
        return handle_;
    }

    static unsigned int hardward_concurrency() noexcept {
        static auto hc = []{ return get_concurrency(); }();
        return hc;
    } 

    // Operations
    void join() {
        join_thread(handle_);
    }

    void detach() {
        detach_thread(handle_);
    }

    void swap(thread& other) noexcept {
        std::swap(handle_, other.handle_);
    }

private:
    native_handle_type  handle_{SYNC_NULL_THREAD};
};

inline void swap(thread& x, thread& y) noexcept {
    x.swap(y);
}

namespace this_thread {
    void yield() noexcept {
        yeild_thread();
    }

    thread::id get_id() noexcept {
        return get_curr_thread_id();
    }

    template<class Rep, class Period>
    void sleep_for(std::chrono::duration<Rep, Period> const& sleep_duration) {
        thread_sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>{sleep_duration});
    }

    template<class Clock, class Duration>
    void sleep_until(std::chrono::time_point<Clock, Duration> const& sleep_time) {
        sleep_for(sleep_time - Clock::now());
    }
}

/*
TODO:
class stop_token;
class stop_source;
class stop_callback
class jthread;
*/

}