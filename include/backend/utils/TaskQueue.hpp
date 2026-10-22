#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "logging/logging.h"

// Runs tasks one after another on a single thread. Everything posted here can use the
// backend data without locking.
class TaskQueue
{
   private:
    LoggerFramework::LogEx lg;
    std::mutex mutex;
    std::condition_variable wakeUp;
    std::deque<std::function<void()>> tasks;
    bool stopping = false;
    // Declared last, the thread must only start once the members above exist
    std::thread thread;

    void run ()
    {
        for(;;)
        {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex);
                wakeUp.wait(lock, [this] { return stopping || !tasks.empty(); });
                // Pending tasks still run on stop, so the last user input is not lost
                if(tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop_front();
            }

            try
            {
                task();
            }
            catch(const std::exception& e)
            {
                LOG_ERROR(lg) << "task failed: " << e.what();
            }
        }
    }

   public:
    TaskQueue ()
        : lg("TaskQueue")
        , thread([this] { run(); })
    {
    }

    ~TaskQueue () { stop(); }

    // Ignored after stop()
    void post (std::function<void()> task)
    {
        {
            std::lock_guard lock(mutex);
            if(stopping) return;
            tasks.push_back(std::move(task));
        }
        wakeUp.notify_one();
    }

    // Runs the pending tasks and joins the thread
    void stop ()
    {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        wakeUp.notify_one();
        if(thread.joinable()) thread.join();
    }
};
