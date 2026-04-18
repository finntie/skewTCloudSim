#include "tools/thread_pool.hpp"

using namespace bee;

ThreadPool::ThreadPool(std::size_t numberOfThreads) : m_stopped(false)
{
    m_threads.reserve(numberOfThreads);
    for (std::size_t i = 0; i < numberOfThreads; ++i)
    {
        m_threads.emplace_back(
            [this]
            {
                while (true)
                {
                    std::packaged_task<void()> task;
                    {
                        std::unique_lock<std::mutex> uniqueLock(m_mutex);
                        m_condition.wait(uniqueLock, [this] { return m_tasks.empty() == false || m_stopped == true; });
                        if (m_tasks.empty() == false)
                        {
                            task = std::move(m_tasks.front());
                            // attention! tasks_.front() moved
                            m_tasks.pop();
                        }
                        else
                        {  // stopped_ == true (necessarily)
                            return;
                        }
                    }
                    task();
                }
            });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lockGuard(m_mutex);
        m_stopped = true;
    }
    m_condition.notify_all();
    for (std::thread& worker : m_threads)
    {
        worker.join();
    }
}
