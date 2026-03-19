#pragma once

#include <functional>
#include <future>
#include <queue>

namespace bee
{

/// <summary>
/// A thread pool that can be used to execute tasks in parallel.
/// Taken (with modifications) from the interwebs, hence the different naming convention.
/// </summary>
class ThreadPool
{
public:
    ThreadPool(std::size_t numberOfThreads);
    ~ThreadPool();  // joins all threads

    template <class F, class... A>
    decltype(auto) Enqueue(F &&callable, A &&...arguments);

    size_t NumberOfThreads() const { return m_threads.size(); }

private:
    std::vector<std::thread> m_threads;
    std::queue<std::packaged_task<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_stopped;
};

template <class F, class... A>
decltype(auto) ThreadPool::Enqueue(F &&callable, A &&...arguments)
{
    using ReturnType = std::invoke_result_t<F, A...>;
    std::packaged_task<ReturnType()> task(std::bind(std::forward<F>(callable), std::forward<A>(arguments)...));
    std::future<ReturnType> taskFuture = task.get_future();
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_tasks.emplace(std::move(task));
        // attention! task moved
    }
    m_condition.notify_one();
    return taskFuture;
}

}  // namespace bee