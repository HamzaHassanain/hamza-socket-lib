#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <atomic>
#include <iostream>

class thread_pool
{
public:
    /**
     * @brief Construct a new thread pool object
     *
     * @param num_threads number of worker threads (thread pool size)
     */
    thread_pool(size_t num_threads)
    {
        stop.store(false);

        for (size_t i = 0; i < num_threads; ++i)
        {
            workers.emplace_back([this]()
                                 {
                                         while (!stop.load())
                                         {
                                             std::function<void()> task;
                                             {
                                                 std::unique_lock<std::mutex> lock(queue_mutex);
                                                 condition.wait(lock, [this] { return stop.load() || !tasks.empty(); });
                                                 if (stop.load() && tasks.empty())
                                                     return;
                                                 task = std::move(tasks.front());
                                                 tasks.pop();
                                             }
                                             task();
                                         } });
        }
    }
    ~thread_pool()
    {
        std::cout << "Stopping thread pool..." << std::endl;
        stop.store(true);
        condition.notify_all();
        for (std::thread &worker : workers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    template <typename F>
    void enqueue(F &&f)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    void stop_workers()
    {
        stop.store(true);
        condition.notify_all();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};