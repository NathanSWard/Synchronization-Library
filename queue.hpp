#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

namespace sync {

template<class T, class Allocator = std::allocator<T>>
class simple_blocking_queue {
public:
    template<class ...Args>
    void push(Args&&... args) {
        {
            std::scoped_lock lock{mutex_};
            queue_.emplace(std::forward<Args>(args)...);
        }
        ready_.notify_one();
    }

    template<class ...Args>
    [[nodiscard]] 
    bool try_push(Args&&... args) {
        {
            std::unique_lock lock{mutex_, std::try_to_lock};
            if (!lock) 
                return false;
            queue_.emplace(std::forward<Args>(args)...);
        }
        ready_.notify_one();
        return true;
    }

    [[nodiscard]] 
    std::optional<T> pop(T& item) {
        std::unique_lock lock{mutex_};
        ready_.wait(lock, [this]{return !queue_.empty() || done_;});
        if (queue_.empty()) 
            return {};

        if constexpr (std::is_move_constructible_v<T>)
            auto item = std::move(queue_.front());
        else
            auto item = queue_.front();
        
        queue_.pop();
        return item;
    }

    [[nodiscard]] 
    std::optional<T> try_pop() {
        std::unique_lock lock{mutex_, std::try_to_lock};
        if (!lock || queue_.empty()) 
            return {};

        if constexpr (std::is_move_constructible_v<T>)
            auto item = std::move(queue_.front());
        else
            auto item = queue_.front();

        queue_.pop();
        return item;
    }

    void done() noexcept {
        {
            std::scoped_lock lock{mutex_};
            done_ = true;
        }
        ready_.notify_all();
    }

    [[nodiscard]] 
    bool empty() const noexcept {
        std::scoped_lock lock{mutex_};
        return queue_.empty();
    }

    [[nodiscard]] 
    unsigned int size() const noexcept {
        std::scoped_lock lock{mutex_};
        return queue_.size();
    }

private:
    std::queue<T, std::deque<T, Allocator>> queue_;
    std::condition_variable                 ready_;
    mutex                                   mutex_;
    bool                                    done_{false};
};

template<class T, 
        class Semaphore,
        class Mutex,
        class Allocator = std::allocator<T>>
class blocking_queue {
public:
    explicit blocking_queue(unsigned int size)
        : open_slots_{size}
		, data_{static_cast<T*>(Allocator{}.allocate(size))}
        , size_{size}
    {
        assert(size != 0);
    }

    ~blocking_queue() {
        std::scoped_lock lock{mutex_};
        while (count_--) {
            data_[pop_index_].~T();
            pop_index_ = ++pop_index_ % size_;
        }
		Allocator{}.deallocate(data_, size_);
    }

    template<class ...Args>
    void push(Args&&... args) noexcept {
        open_slots_.wait();
        {
            std::scoped_lock lock{mutex_};
            new(data_ + push_index_) T(std::forward<Args>(args)...);
            push_index_ = ++push_index_ % size_;
            ++count_;
        }
        full_slots_.post();
    }

    template<class ...Args>
    [[nodiscard]]
    bool try_push(Args&&... args) noexcept {
        if (!open_slots_.wait_for(std::chrono::seconds(0))) 
            return false;
        {
            std::scoped_lock lock{mutex_};
            new(data_ + push_index_) T(std::forward<Args>(args)...);
            push_index_ = ++push_index_ % size_;
            ++count_;
        }
        full_slots_.post();
        return true;
    }

    template<class U>
    void pop(U& item) noexcept {
        full_slots_.wait();
        {
            std::scoped_lock lock{mutex_};

            if constexpr (std::is_assignable_v<U, T&&>)
                item = std::move(data_[pop_index_]);
            else 
                item = data_[m_popIntex];

            data_[pop_index_].~T();
            pop_index_ = ++pop_index_ % size_;
            --count_;
        }
        open_slots_.post();
    }

    [[nodiscard]]
    std::optional<T> try_pop() noexcept {
        if (!full_slots_.wait_for(std::chrono::seconds(0))) 
            return {};

        std::optional<T> opt;
        {
            std::scoped_lock lock{mutex_};

            if constexpr (std::is_move_constructible_v<T>)
                opt.emplace(std::move(data_[pop_index_]));
            else 
                opt.emplace(data_[m_popIntex]);

            data_[pop_index_].~T();
            pop_index_ = ++pop_index_ % size_;
            --count_;
        }
        open_slots_.post();
        return opt;
    }

    [[nodiscard]]
    bool empty() const noexcept {
        std::scoped_lock lock{mutex_};
        return count_ == 0;
    }

    [[nodiscard]]
    bool full() const noexcept {
        std::scoped_lock lock{mutex_};
        return count_ == size_;
    }

    [[nodiscard]]
    unsigned int size() const noexcept {
        std::scoped_lock lock{mutex_};
        return count_;
    }

    [[nodiscard]]
    unsigned int capacity() const noexcept {
        return size_;
    }

private:
    Semaphore           open_slots_;
    Semaphore           full_slots_{0};
    Mutex               mutex_;
    T*                  data_;
    unsigned int const  size_;
    unsigned int        push_index_{0};
    unsigned int        pop_index_{0};
    unsigned int        count_{0};
};

template<class T, 
        class Semaphore,
        class Allocator = std::allocator<T>>
class lock_free_queue {
public:
    explicit lock_free_queue(unsigned int size)
        : open_slots_{size}
        , data_{static_cast<T*>(Allocator{}.allocate(size))}
        , size_{size}
    {
        assert(size != 0);
    }

    ~lock_free_queue() noexcept {
        while (count_--) {
            data_[pop_index_].~T();
            pop_index_ = ++pop_index_ % size_;
        }
		Allocator{}.deallocate(data_, size_);
    }

    template<class ...Args>
    void push(Args&&... args) noexcept {
        open_slots_.wait();

        new(data_ + (push_index_.fetch_add(1, std::memory_order_release) % size_)) T(std::forward<Args>(args)...);
        count_.fetch_add(1, std::memory_order_relaxed);

        auto expected = push_index_.load(std::memory_order_acquire);
        while (!push_index_.compare_exchange_weak(expected, push_index_.load(std::memory_order_acquire) % size_, std::memory_order_release, std::memory_order_relaxed))
            expected = push_index_.load(std::memory_order_acquire);

        full_slots_.post();
    }

    template<class ...Args>
    [[nodiscard]]
    bool try_push(Args&&... args) noexcept {
        if(!open_slots_.wait_for(std::chrono::seconds(0))) 
            return false;

        new(data_ + (push_index_.fetch_add(1, std::memory_order_release) % size_)) T(std::forward<Args>(args)...);
        count_.fetch_add(1, std::memory_order_relaxed);

        auto expected = push_index_.load(std::memory_order_acquire);
        while (!push_index_.compare_exchange_weak(expected, push_index_.load(std::memory_order_acquire) % size_, std::memory_order_release, std::memory_order_relaxed))
            expected = push_index_.load(std::memory_order_acquire);

        full_slots_.post();
        return true;
    }

    [[nodiscard]]
    T pop() noexcept {
        full_slots_.wait();

        auto popIndex = pop_index_.fetch_add(1, std::memory_order_release);

        if constexpr (std::is_move_constructible_v<T>)
            auto item = std::move(data_[popIndex % size_])
        else
            auto item = data_[popIndex % size_];

        data_[popIndex % size_].~T();
        count_.fetch_sub(1, std::memory_order_relaxed);

        auto expected = pop_index_.load(std::memory_order_acquire);
        while (!pop_index_.compare_exchange_weak(expected, pop_index_.load(std::memory_order_acquire) % size_, std::memory_order_release, std::memory_order_relaxed))
            expected = pop_index_.load(std::memory_order_acquire);

        open_slots_.post();
        return item;
    }

    [[nodiscard]]
    std::optional<T> try_pop() noexcept {
		if (!full_slots_.wait_for(std::chrono::seconds{0}))
            return false;

        std::optional<T> opt;
        auto popIndex = pop_index_.fetch_add(1, std::memory_order_release);

        if constexpr (std::is_assignable_v<U, T&&>)
            opt.emplace(std::move(data_[popIndex % size_]));
        else
            opt.emplace(data_[popIndex % size_]);

        data_[popIndex % size_].~T();
        count_.fetch_sub(1, std::memory_order_relaxed);

        auto expected = pop_index_.load(std::memory_order_acquire);
        while (!pop_index_.compare_exchange_weak(expected, pop_index_.load(std::memory_order_acquire) % size_, std::memory_order_release, std::memory_order_relaxed))
            expected = pop_index_.load(std::memory_order_acquire);

        open_slots_.post();
        return true;
    }

    [[nodiscard]]
    bool empty() const noexcept {
        return count_.load(std::memory_order_relaxed) == 0;
    }

    [[nodiscard]]
    bool full() const noexcept {
        return count_.load(std::memory_order_relaxed) == size_;
    }

    [[nodiscard]]
    unsigned int size() const noexcept {
        return count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]]
    unsigned int capacity() const noexcept {
        return size_;
    }

private:
    Semaphore           open_slots_;
    Semaphore           full_slots_{0};
    T*                  data_;
    std::atomic_uint    push_index_{0};
    std::atomic_uint    pop_index_{0};
    std::atomic_uint    count_{0};
    unsigned int const  size_;
};

} // namespace sync