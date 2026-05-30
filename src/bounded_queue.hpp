// bounded_queue.hpp - bounded thread-safe queue for the producer/consumer split.
//
// The producer never blocks: when the consumer falls behind, the oldest item
// is dropped so latency stays bounded and playback tracks the live stream.
#pragma once
#include <deque>
#include <mutex>
#include <condition_variable>

template <class T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity = 2) : cap_(capacity) {}

    // Returns the number of items dropped to make room (0 or 1).
    int push(T&& item) {
        int dropped = 0;
        {
            std::lock_guard<std::mutex> lk(m_);
            while (q_.size() >= cap_) { q_.pop_front(); ++dropped; }
            q_.push_back(std::move(item));
        }
        cv_.notify_one();
        return dropped;
    }

    // Blocks until an item is available or the queue is closed+drained.
    bool pop(T& out) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    size_t size() {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

    void close() {
        { std::lock_guard<std::mutex> lk(m_); closed_ = true; }
        cv_.notify_all();
    }

private:
    std::deque<T>           q_;
    std::mutex              m_;
    std::condition_variable cv_;
    size_t                  cap_;
    bool                    closed_ = false;
};
