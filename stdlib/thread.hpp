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
        initialize_thread(_handle, 
            [&](void*) -> void* {std::forward<Function>(f)(std::forward<Args>(args)...); return nullptr;},
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
        get_concurrency();
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
    // TODO
}

/*
TODO:
class stop_token;
class stop_source;
class stop_callback
class jthread;
*/

}