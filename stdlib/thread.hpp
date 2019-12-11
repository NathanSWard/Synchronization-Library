// thread.hpp
#pragma once

#include "_thread.hpp"
#include "stop_token.hpp"

#include <functional>
#include <utility>

namespace sync {

class jthread { // class for self-joining threads
public:
    using id = thread::id;
    using native_handle_type = thread::native_handle_type;

    jthread() noexcept 
        : stop_source_{nostopstate}
        , thread_{} 
    {}

    template <class Fn, class... Args, std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<Fn>>, jthread>, int> = 0>
    explicit jthread(Fn&& fn, Args&&... args)
        : stop_source_{}
        , thread_{[](auto&& fn2, stop_token token, auto&&... args2) {
                        if constexpr (std::is_invocable_v<Fn, stop_token, Args...>)
                            std::invoke(std::forward<decltype(fn2)>(fn2), std::move(token), std::forward<decltype(args2)>(args2)...); 
                        else
                            std::invoke(std::forward<decltype(fn2)>(fn2), std::forward<decltype(args2)>(args2)...);
                    },
                    std::forward<Fn>(fn), stop_source_.get_token(), std::forward<Args>(args)...} 
    {}

    ~jthread() noexcept {
        if (joinable()) {
            request_stop();
            join();
        }
    }

    jthread(jthread&& _Other) noexcept = default;
    jthread& operator=(jthread&& _Other) noexcept = default;

    jthread(const jthread&) = delete;
    jthread& operator=(const jthread&) = delete;

    void swap(jthread& _Other) noexcept {
        thread_.swap(_Other.thread_);
        stop_source_.swap(_Other.stop_source_);
    }

    void detach() {
        thread_.detach();
    }

    id get_id() const noexcept {
        return thread_.get_id();
    }

    static auto hardware_concurrency() noexcept {
        return thread::hardware_concurrency();
    }

    void join() {
        thread_.join();
    }

    bool joinable() const noexcept {
        return thread_.joinable();
    }

    native_handle_type native_handle() {
        return thread_.native_handle();
    }

    [[nodiscard]] stop_source get_stop_source() noexcept {
        return stop_source_;
    }

    [[nodiscard]] stop_token get_stop_token() const noexcept {
        return stop_source_.get_token();
    }

    bool request_stop() noexcept {
        return stop_source_.request_stop();
    }

private:
    stop_source stop_source_;
    thread thread_;
};

inline void swap(jthread& lhs, jthread& rhs) noexcept {
    lhs.swap(rhs);
}

} // namespace sync
