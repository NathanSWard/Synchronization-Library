#pragma once

#include "stdlib/condition_variable.hpp"
#include "stdlib/mutex.hpp"

namespace sync {

class manual_event {
public:
    explicit manual_event(bool signaled = false) noexcept
        : signaled_{signaled}
    {}

    void signal() noexcept {
        {
            scoped_lock lock{mutex_};
            signaled_ = true;
        }
        cv_.notify_all();
    }

    void wait() noexcept {
        unique_lock lock{mutex_};
        cv_.wait(lock, [&](){ return signaled_ != false; });
    }

    template<class Duration>
    [[nodiscard]]
    bool wait_for(Duration&& d) noexcept {
        unique_lock lock{mutex_};
        return cv_.wait_for(lock, d, [&](){ return signaled_ != false; });
    }

    template<class Duration>
    [[nodiscard]]
    bool wait_until(Duration&& d) noexcept {
        unique_lock lock{mutex_};
        return cv_.wait_until(lock, d, [&](){ return signaled_ != false; });
    }

    void reset() noexcept {
        scoped_lock lock{mutex_};
        signaled_ = false;
    }

private:
    condition_variable cv_;
    mutex              mutex_;
    bool               signaled_;
};

class auto_event {
public:
    explicit auto_event(bool signaled = false) noexcept
        : signaled_{signaled}
    {}

    void signal() noexcept {
        {
            scoped_lock lock{mutex_};
            signaled_ = true;
        }
        cv_.notify_one();
    }

    void wait() noexcept {
        unique_lock lock{mutex_};
        cv_.wait(lock, [&](){ return signaled_ != false; });
        signaled_ = false;
    }

    template<class Duration>
    [[nodiscard]]
    bool wait_for(Duration&& d) noexcept {
        unique_lock lock{mutex_};
        bool result = cv_.wait_for(lock, d, [&](){ return signaled_ != false; });
        if (result) 
            signaled_ = false;
        return result;
    }

    template<class Duration>
    [[nodiscard]]
    bool wait_until(Duration&& d) noexcept {
        std::unique_lock lock{mutex_};
        bool result = cv_.wait_until(lock, d, [&](){ return signaled_ != false; });
        if (result) 
            signaled_ = false;
        return result;
    }

private:
    condition_variable cv_;
    mutex              mutex_;
    bool               signaled_;
};

} // namespace sync