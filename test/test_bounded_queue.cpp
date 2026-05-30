// test_bounded_queue.cpp - producer/consumer queue semantics.
#include "test_framework.hpp"
#include "bounded_queue.hpp"
#include <thread>
#include <atomic>

TEST(queue, fifo_order) {
    BoundedQueue<int> q(8);
    q.push(1); q.push(2); q.push(3);
    int v;
    CHECK(q.pop(v)); CHECK_EQ(v, 1);
    CHECK(q.pop(v)); CHECK_EQ(v, 2);
    CHECK(q.pop(v)); CHECK_EQ(v, 3);
}

TEST(queue, drops_oldest_when_full) {
    BoundedQueue<int> q(2);
    CHECK_EQ(q.push(1), 0);
    CHECK_EQ(q.push(2), 0);
    CHECK_EQ(q.push(3), 1);   // one dropped to make room
    int v;
    CHECK(q.pop(v)); CHECK_EQ(v, 2);   // oldest (1) was dropped
    CHECK(q.pop(v)); CHECK_EQ(v, 3);
}

TEST(queue, close_unblocks_consumer) {
    BoundedQueue<int> q(2);
    std::atomic<bool> returned{false};
    std::thread t([&] {
        int v;
        bool ok = q.pop(v);   // blocks until close()
        CHECK(!ok);
        returned = true;
    });
    q.close();
    t.join();
    CHECK(returned.load());
}

TEST(queue, move_only_type) {
    BoundedQueue<std::unique_ptr<int>> q(4);
    q.push(std::make_unique<int>(42));
    std::unique_ptr<int> p;
    CHECK(q.pop(p));
    CHECK(p != nullptr);
    CHECK_EQ(*p, 42);
}
