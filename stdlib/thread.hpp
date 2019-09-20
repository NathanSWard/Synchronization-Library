#pragma once

#include "internal/thread.hpp"

#include <exception>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

// helper namespace for thead::thread construction with function args
namespace {
    template<class T>
    std::decay_t<T> decay_copy(T&& t) {
        return std::forward<T>(t);
    }
    
    template<class Fp, class... Args, std::size_t ...Is>
    inline void call_void_callable(std::tuple<Fp, Args...>& tup, std::index_sequence<Is...>) {
        std::invoke(std::move(std::get<Is>(tup))...); 
    }

    template<class T>
    void* void_callable(void* args) {
        std::unique_ptr<T> ptr{static_cast<T*>(args)};
        using idx = std::make_index_sequence<std::tuple_size_v<T>>;
        call_void_callable(*ptr, idx{});
        return nullptr;
    }
}

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
        return *this;
    }

    template <class Fn, class ...Args>
    explicit thread(Fn&& f, Args&&... args) {
        using Tup = std::tuple<std::decay_t<Fn>, std::decay_t<Args>...>;  
        std::unique_ptr<Tup> ptr{
            std::make_unique<Tup>(decay_copy(std::forward<Fn>(f)), 
                                  decay_copy(std::forward<Args>(args))...)};
        initialize_thread(handle_, &void_callable<Tup>, static_cast<void*>(ptr.release()));
    }

    ~thread() {
        if (joinable())
            std::terminate();
    }

    // Observers
    bool joinable() {
        return !is_thread_null(handle_);
    }

    id get_id() const noexcept {
        return get_thread_id(handle_);
    }

    native_handle_type native_handle() {
        return handle_;
    }

    static unsigned int hardward_concurrency() noexcept {
        return get_concurrency();
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
        thread_sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_duration));
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
