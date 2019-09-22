#pragma once

#include <functional>
#include <future>
#include <thread>
#include <vector>

namespace sync {

template<template<class> class Queue>
class simple_thread_pool {
public:
    explicit simple_thread_pool(unsigned int num_threads_) {
        assert(num_threads_ != 0);
        threads_.reserve(num_threads_);

        for (unsigned int i = 0; i < num_threads_; ++i)
            threads_.emplace_back([this] {
                std::optional<Proc> proc;
                for (;;) {
                    proc = queue_.pop();
                    if (!proc)
                        break;
                    (*proc)();
                }
            });
    }

    ~simple_thread_pool() noexcept {
        for (auto& thread : threads_)
            thread.join();
    }

    template<typename F, typename... Args>
    void post_work(F&& f, Args&&... args) {
        queue_.push(
            [f = std::forward<F>(f), tup = std::forward_as_tuple(args...)] {
                std::apply([&](auto&&... args){f(args...);}, tup);
            }
        );
    }

    template<typename F, typename... Args>
    auto post_task(F&& f, Args&&... args) -> std::future<std::result_of_t<F(Args...)>> {
        using return_t = std::result_of_t<F(Args...)>;
        auto task = std::make_unique<std::packaged_task<return_t()>>(
            [f = std::forward<F>(f), tup = std::forward_as_tuple(args...)] {
                std::apply([&](auto&&... args){f(args...);}, tup);
            }
        );
        auto result = task->get_future();
        queue_.push([task = std::move(task)] { task->(); });
        return result;
    }

private:
    using Proc = std::function<void()>;
    
    Queue<Proc>                 queue_;
    std::vector<std::thread>    threads_;
};

namespace { constexpr unsigned int LOOP_K = 2; }

template<template<class> class Queue>
class work_stealing_thread_pool {
public:
    explicit work_stealing_thread_pool(unsigned int num_threads)
        : queues_{num_threads}
        , count_{num_threads}
    {
        assert(num_threads != 0);
        auto work = [this](unsigned int i) {
            for (;;) {
                std::optional<Proc> proc;
                for (unsigned int n = 0; n < count_; ++n)
                    if(proc = queues_[(i + n) % count_].try_pop()) 
                        break;
                if (!proc && !(proc = queues_[i].pop())) 
                    break;
                (*proc)();
            }
        };
        for (unsigned int i{0}; i < num_threads; ++i)
            threads_.emplace_back(work, i);
    }

    ~work_stealing_thread_pool() noexcept
    {
        for (auto& thread : threads_)
            thread.join();
    }

    template<typename F, typename... Args>
    void post_work(F&& f, Args&&... args) {
        auto work = [f = std::forward<F>(f), tup = std::forward_as_tuple(args...)] {
            std::apply([&](auto&&... args){f(args...);}, tup);
        };
        unsigned int const i = index_.fetch_add(1, std::memory_order_relaxed);
        for (unsigned int n = 0; n < count_ * LOOP_K; ++n)
            if (queues_[(i + n) % count_].try_push(std::move(work)) 
                return;
        queues_[i % count_].push(std::move(work));
    }

    template<typename F, typename... Args>
    auto post_task(F&& f, Args&&... args) -> std::future<std::result_of_t<F(Args...)>> {
        using return_t = std::result_of_t<F(Args...)>;
        auto task = std::make_unique<std::packaged_task<return_t()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto result = task->get_future();

        auto work = [task = std::move(task)] { task->(); };
        unsigned int const i = index_.fetch_add(1, std::memory_order_relaxed);
        for(unsigned int n = 0; n < count_ * LOOP_K; ++n)
            if(queues_[(i + n) % count_].try_push(std::move(work))) 
                return result;
        queues_[i % count_].push(std::move(work));
        return result;
    }

private:
    using Proc = std::function<void()>;

    std::vector<Queue<Proc>>    queues_;
    std::vector<std::thread>    threads_;
    std::atomic_uint            index_{0};
    unsigned int const          count_;
};

} // namespace sync