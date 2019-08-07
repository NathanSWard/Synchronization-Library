#pragma once

#include "queue.hpp"

#include <functional>
#include <future>
#include <thread>
#include <vector>

namespace sync {

class simple_thread_pool {
public:
    explicit simple_thread_pool(unsigned int num_threads_) {
        assert(num_threads_ != 0);
        threads_.reserve(num_threads_);

        for(unsigned int i = 0; i < num_threads_; ++i)
            threads_.emplace_back([this] {
                for (;;)
                {
                    Proc proc;
                    queue_.pop(proc);
                    if(proc == nullptr)
                    {
                        queue_.push(nullptr);
                        break;
                    }
                    proc();
                }
            });
    }

    ~simple_thread_pool() noexcept {
        queue_.push(nullptr);
        for(auto& thread : threads_)
            thread.join();
    }

    template<typename F, typename... Args>
    void post_work(F&& f, Args&&... args) {
        queue.push(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }

    template<typename F, typename... Args>
    auto post_task(F&& f, Args&&... args) -> std::future<std::result_of_t<F(Args...)>> {
        using return_t = std::result_of_t<F(Args...)>;
        auto task = std::make_unique<std::packaged_task<return_t()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto result = task->get_future();
        queue_.push([task = std::move(task)] { task->(); });
        return result;
    }

private:
    using Proc = std::function<void()>;
    
    simple_blocking_queue<Proc> queue_;
    std::vector<std::thread>    threads_;
};

namespace { constexpr unsigned int LOOP_K = 2; }

class thread_pool {
public:
    explicit thread_pool(unsigned int num_threads)
        : queues_{num_threads}
        , count_{num_threads}
    {
        assert(num_threads != 0);
        auto work = [this](unsigned int i) {
            for(;;) {
                Proc proc;
                for(unsigned int n = 0; n < count_; ++n)
                    if(queues_[(i + n) % count_].try_pop(proc)) 
                        break;
                if(!proc && !queues_[i].pop(proc)) 
                    break;
                proc();
            }
        };
        for(unsigned int i = 0; i < num_threads; ++i)
            threads_.emplace_back(work, i);
    }

    ~thread_pool() noexcept
    {
        for(auto& queue : queues_)
            queue.done();
        for(auto& thread : threads_)
            thread.join();
    }

    template<typename F, typename... Args>
    void post_work(F&& f, Args&&... args) {
        auto work = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        unsigned int const i = index_.fetch_add(1, std::memory_order_relaxed);
        for(unsigned int n = 0; n < count_ * LOOP_K; ++n)
            if(queues_[(i + n) % count_].try_push(std::move(work)) 
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

    std::vector<simple_blocking_queue<Proc>>    queues_;
    std::vector<std::thread>                    threads_;
    std::atomic_uint                            index_{0};
    unsigned int const                          count_;
};

} // namespace sync