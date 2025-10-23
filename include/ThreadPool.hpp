#pragma once

#include <functional>
#include <thread>

#include "Log.hpp"

namespace SpotifySearch {

class ThreadPool {
    public:
    struct Job {
        size_t id_;
        std::function<void()> task_;

        bool operator==(const Job& other) const {
            return id_ == other.id_;
        }
    };

    void submit(const std::function<void()>& task);

    void wait();

    private:
    size_t maxThreadCount_ = std::thread::hardware_concurrency();
    std::vector<Job> jobs_;
    std::mutex mutex_;
    std::condition_variable conditionVariable_;

    void onJobFinished(const Job& job);
};

}// namespace SpotifySearch
