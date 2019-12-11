// stop_token.hpp
#pragma once

#include "_thread.hpp"

#include <atomic>
#include <type_traits>
#include <utility>

namespace sync {

struct _stop_callback_node { // stack of stop_callbacks
    void (*callback_ptr_)(_stop_callback_node&) = nullptr;
    _stop_callback_node* next_                  = nullptr;
    _stop_callback_node** prev_                 = nullptr;
    bool* is_removed_                           = nullptr;
    std::atomic<bool> callback_finished_executing_{false};

    void execute() noexcept {
        callback_ptr_(*this);
    }

protected:
    ~_stop_callback_node() = default;
};

struct _stop_state { // Concurrent handeling of stop_token/source/callback
public:
    void increment_token_ref() noexcept {
        state_.fetch_add(stop_token_increment_, std::memory_order_relaxed);
    }

    void decrement_token_ref() noexcept {
        auto const past_state = state_.fetch_sub(stop_token_increment_, std::memory_order_acq_rel);
        if (past_state < (stop_token_increment_ + stop_token_increment_))
            delete this;
    }

    void increment_source_ref() noexcept {
        state_.fetch_add(stop_source_increment_, std::memory_order_relaxed);
    }

    void decrement_source_ref() noexcept {
        auto const past_state = state_.fetch_sub(stop_source_increment_, std::memory_order_acq_rel);
        if (past_state < (stop_token_increment_ + stop_source_increment_))
            delete this;
    }

    bool request_stop() noexcept {

        if (!try_request_stop_andlock())
            return false;

        thread_id_ = this_thread::get_id();

        while (head_ != nullptr) {
            auto* cb = head_;
            head_ = cb->next_;
            bool const empty = (head_ == nullptr);
            if (!empty)
                head_->prev_ = &head_;

            cb->prev_ = nullptr;
            unlock();

            bool is_removed_ = false;
            cb->is_removed_ = &is_removed_;
            cb->execute();

            if (!is_removed_) {
                cb->is_removed_ = nullptr;
                cb->callback_finished_executing_.store(true, std::memory_order_release);
            }

            if (empty)
                return true;

            lock();
        }

        unlock();

        return true;
    }

    bool stop_requested() noexcept {
        return stop_requested(state_.load(std::memory_order_acquire));
    }

    bool stop_possible() noexcept {
        return stop_possible(state_.load(std::memory_order_acquire));
    }

    bool try_push_callback(_stop_callback_node* cb, bool const is_stop_token_copy) noexcept {
        uint64_t past_state; // uninitialized

        do { // attempt to lock state_
            for (;;) {
                past_state = state_.load(std::memory_order_acquire);
                if (stop_requested(past_state)) {
                    cb->execute();
                    return false;
                }
                if (!stop_possible(past_state))
                    return false;
                if (!is_locked(past_state))
                    break;
                this_thread::yield();
            }
        } while (!state_.compare_exchange_weak(past_state, past_state | locked_bit_, std::memory_order_acquire));

        cb->next_ = head_;
        if (cb->next_ != nullptr)
            cb->next_->prev_ = &cb->next_;
        cb->prev_ = &head_;
        head_ = cb;

        if (is_stop_token_copy)
            unlock_and_increment_token_ref();
        else // moved stop_token
            unlock();

        return true;
    }

    void erase_callback(_stop_callback_node* cb) noexcept {
        lock();

        if (cb->prev_ != nullptr) { // remove callback from list
            *cb->prev_ = cb->next_;
            if (cb->next_ != nullptr) // repair list
                cb->next_->prev_ = cb->prev_;
            unlock_and_decrement_token_ref();
            return;
        }

        unlock();

        if (thread_id_ == this_thread::get_id()) { // callback is executing or has executed on this thread
            if (cb->is_removed_ != nullptr) // synchronize with request_stop
                *cb->is_removed_ = true;
        } else {
            while (!cb->callback_finished_executing_.load(std::memory_order_acquire)) { // block
                this_thread::yield();
            }
        }
        
        decrement_token_ref();
    }

private:
    bool is_locked(uint64_t const state) const noexcept {
        return (state & locked_bit_) != 0;
    }

    bool stop_requested(uint64_t const state) const noexcept {
        return (state & stop_requested_bit_) != 0;
    }

    bool stop_possible(uint64_t const state) const noexcept {
        return stop_requested(state) || (state >= stop_source_increment_);
    }

    bool try_request_stop_andlock() noexcept {
        auto past_state = state_.load(std::memory_order_acquire);
        do {
            if (stop_requested(past_state))
                return false;
            while (is_locked(past_state)) {
                this_thread::yield();
                past_state = state_.load(std::memory_order_acquire);
                if (stop_requested(past_state)) {
                    return false;
                }
            }
        } while (!state_.compare_exchange_weak(
            past_state, past_state | stop_requested_bit_ | locked_bit_, std::memory_order_acq_rel, std::memory_order_acquire));
        return true;
    }

    void lock() noexcept { // test and test-and-set
        uint64_t past_state = state_.load(std::memory_order_relaxed);
        do {
            while (is_locked(past_state)) {
                this_thread::yield();
                past_state = state_.load(std::memory_order_relaxed);
            }
        } while (!state_.compare_exchange_weak(
            past_state, past_state | locked_bit_, std::memory_order_acquire, std::memory_order_relaxed));
    }

    void unlock() noexcept {
        state_.fetch_and(~locked_bit_, std::memory_order_release);
    }

    void unlock_and_increment_token_ref() noexcept {
        state_.fetch_sub(locked_bit_ - stop_token_increment_, std::memory_order_release);
    }

    void unlock_and_decrement_token_ref() noexcept {
        auto const past_state = state_.fetch_sub(locked_bit_ + stop_token_increment_, std::memory_order_acq_rel);
        if (past_state < (locked_bit_ + stop_token_increment_ + stop_token_increment_))
            delete this;
    }

    static constexpr uint64_t stop_requested_bit_ = 0b1;
    static constexpr uint64_t locked_bit_ = 0b10;
    static constexpr uint64_t stop_token_increment_ = 0b100;
    static constexpr uint64_t stop_source_increment_ = static_cast<uint64_t>(1) << 33;

    std::atomic<uint64_t> state_ = stop_source_increment_;
    _stop_callback_node* head_ = nullptr;
    thread::id thread_id_{};
};

// CLASS stop_token
class stop_token {
public:
    stop_token() noexcept {}

    ~stop_token() {
        if (stop_state_ != nullptr)
            stop_state_->decrement_token_ref();
    }

    stop_token(stop_token&& other) noexcept 
        : stop_state_{std::exchange(other.stop_state_, nullptr)} 
    {}

    stop_token& operator=(stop_token&& other) noexcept {
        stop_token token{std::move(other)};
        swap(token);
        return *this;
    }

    stop_token(stop_token const& other) noexcept 
        : stop_state_{other.stop_state_} 
    {
        if (stop_state_ != nullptr)
            stop_state_->increment_token_ref();
    }

    stop_token& operator=(stop_token const& other) noexcept {
        if (stop_state_ != other.stop_state_) {
            stop_token token{other};
            swap(token);
        }
        return *this;
    }

    void swap(stop_token& other) noexcept {
        std::swap(stop_state_, other.stop_state_);
    }

    [[nodiscard]] bool stop_requested() const noexcept {
        return stop_state_ != nullptr && stop_state_->stop_requested();
    }

    [[nodiscard]] bool stop_possible() const noexcept {
        return stop_state_ != nullptr && stop_state_->stop_possible();
    }

    [[nodiscard]] friend bool operator==(stop_token const& lhs, stop_token const& rhs) noexcept {
        return lhs.stop_state_ == rhs.stop_state_;
    }
    [[nodiscard]] friend bool operator!=(stop_token const& lhs, stop_token const& rhs) noexcept {
        return lhs.stop_state_ != rhs.stop_state_;
    }

private:
    friend class stop_source;
    template <class Callback> friend class stop_callback;

    explicit stop_token(_stop_state* state) noexcept 
        : stop_state_{state} 
    {
        if (stop_state_ != nullptr)
            stop_state_->increment_token_ref();
    }

    _stop_state* stop_state_{nullptr};
};

inline void swap(stop_token& lhs, stop_token& rhs) noexcept {
    lhs.swap(rhs);
}

struct nostopstate_t {
    explicit nostopstate_t() = default;
};

inline constexpr nostopstate_t nostopstate{};

// CLASS stop_source
class stop_source {
public:
    stop_source() 
        : stop_state_{new _stop_state} 
    {}

    explicit stop_source(nostopstate_t) noexcept 
        : stop_state_{nullptr} 
    {}

    ~stop_source() {
        if (stop_state_ != nullptr)
            stop_state_->increment_source_ref();
    }

    stop_source(stop_source&& other) noexcept 
        : stop_state_{std::exchange(other.stop_state_, nullptr)} 
    {}

    stop_source& operator=(stop_source&& other) noexcept {
        stop_source _Source{std::move(other)};
        swap(_Source);
        return *this;
    }

    stop_source(stop_source const& other) noexcept 
        : stop_state_{other.stop_state_} 
    {
        if (stop_state_ != nullptr)
            stop_state_->increment_source_ref();
    }

    stop_source& operator=(stop_source const& other) noexcept {
        if (stop_state_ != other.stop_state_) {
            stop_source _Source{other};
            swap(_Source);
        }
        return *this;
    }

    void swap(stop_source& other) noexcept {
        std::swap(stop_state_, other.stop_state_);
    }

    [[nodiscard]] stop_token get_token() const noexcept {
        return stop_token{stop_state_};
    }

    [[nodiscard]] bool stop_possible() const noexcept {
        return stop_state_ != nullptr;
    }

    [[nodiscard]] bool stop_requested() const noexcept {
        return stop_state_ != nullptr && stop_state_->stop_requested();
    }

    bool request_stop() noexcept {
        if (stop_state_ != nullptr)
            return stop_state_->request_stop();
        return false;
    }

    [[nodiscard]] friend bool operator==(stop_source const& lhs, stop_source const& rhs) noexcept {
        return lhs.stop_state_ == rhs.stop_state_;
    }

    [[nodiscard]] friend bool operator!=(stop_source const& lhs, stop_source const& rhs) noexcept {
        return lhs.stop_state_ != rhs.stop_state_;
    }

private:
    _stop_state* stop_state_;
};

inline void swap(stop_source& lhs, stop_source& rhs) noexcept {
    lhs.swap(rhs);
}

// CLASS TEMPLATE stop_callback
template <typename Callback>
class stop_callback : private _stop_callback_node {
public:
    using callback_type = Callback;

    template <class Fn, std::enable_if_t<std::is_constructible_v<Callback, Fn>, int> = 0>
    explicit stop_callback(stop_token const& token, Fn&& fn) noexcept(std::is_nothrow_constructible_v<Callback, Fn>)
        : _stop_callback_node{[](_stop_callback_node& node) noexcept {
              static_cast<stop_callback&>(node).execute();
          }}
        , stop_state_{nullptr}
        , callback_{static_cast<Fn&&>(fn)} 
    {
        if (token.stop_state_ != nullptr && token.stop_state_->try_push_callback(this, true))
            stop_state_ = token.stop_state_;
    }

    template <class Fn, std::enable_if_t<std::is_constructible_v<Callback, Fn>, int> = 0>
    explicit stop_callback(stop_token&& token, Fn&& fn) noexcept(std::is_nothrow_constructible_v<Callback, Fn>)
        : _stop_callback_node{[](_stop_callback_node& node) noexcept {
              static_cast<stop_callback&>(node).execute();
          }}
        , stop_state_{nullptr}
        , callback_{static_cast<Fn&&>(fn)} 
    {
        if (token.stop_state_ != nullptr && token.stop_state_->try_push_callback(this, false))
            stop_state_ = std::exchange(token.stop_state_, nullptr);
    }

    ~stop_callback() {
        if (stop_state_ != nullptr)
            stop_state_->erase_callback(this);
    }

    stop_callback(stop_callback&&) = delete;
    stop_callback& operator=(stop_callback&&) = delete;

    stop_callback(const stop_callback&) = delete;
    stop_callback& operator=(const stop_callback&) = delete;

private:
    void execute() noexcept {
        static_cast<void>(callback_());
    }

    _stop_state* stop_state_;
    Callback callback_;
};

template <class Callback>
stop_callback(stop_token, Callback)->stop_callback<Callback>;

} // namespace sync