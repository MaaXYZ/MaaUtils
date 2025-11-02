#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cassert>

#include "MaaUtils/StaticEventBus.hpp"

using namespace MAA_NS;

// Test event types - each test uses its own event type to avoid interference
class Event1 : public Event {
public:
    int value;
    explicit Event1(int v) : value(v) {}
};

class Event2 : public Event {
public:
    int value;
    explicit Event2(int v) : value(v) {}
};

class Event3 : public Event {
public:
    int value;
    explicit Event3(int v) : value(v) {}
};

class Event4 : public Event {
public:
    int value;
    explicit Event4(int v) : value(v) {}
};

class Event5 : public Event {
public:
    int value;
    explicit Event5(int v) : value(v) {}
};

class Event6 : public CancellableEvent {
public:
    int value;
    explicit Event6(int v) : value(v) {}
};

class Event7 : public Event {
public:
    int value;
    explicit Event7(int v) : value(v) {}
};

class Event8 : public Event {
public:
    int depth;
    explicit Event8(int d) : depth(d) {}
};

class Event9 : public Event {
public:
    int value;
    explicit Event9(int v) : value(v) {}
};

// Test subscriber class
class TestSubscriber {
public:
    int call_count = 0;
    int last_value = 0;

    void on_event(const Event4& event) {
        call_count++;
        last_value = event.value;
    }

    void on_cancellable_event(const Event6& event) {
        call_count++;
        last_value = event.value;
    }
};

// Test 1: Basic subscription and publish
void test_basic_subscription() {
    std::cout << "Test 1: Basic subscription and publish... ";
    
    int call_count = 0;
    int received_value = 0;

    StaticEventManager<DefaultMutex>::subscribe<Event1>(
        [&](const Event1& event) {
            call_count++;
            received_value = event.value;
        }
    );

    Event1 event(42);
    StaticEventManager<DefaultMutex>::publish(event);

    assert(call_count == 1);
    assert(received_value == 42);
    
    std::cout << "PASSED\n";
}

// Test 2: Multiple subscribers
void test_multiple_subscribers() {
    std::cout << "Test 2: Multiple subscribers... ";
    
    std::atomic<int> call_count{0};

    StaticEventManager<DefaultMutex>::subscribe<Event2>(
        [&](const Event2&) { call_count++; }
    );
    StaticEventManager<DefaultMutex>::subscribe<Event2>(
        [&](const Event2&) { call_count++; }
    );
    StaticEventManager<DefaultMutex>::subscribe<Event2>(
        [&](const Event2&) { call_count++; }
    );

    Event2 event(100);
    StaticEventManager<DefaultMutex>::publish(event);

    assert(call_count == 3);
    
    std::cout << "PASSED\n";
}

// Test 3: Priority ordering
void test_priority_ordering() {
    std::cout << "Test 3: Priority ordering... ";
    
    std::vector<int> call_order;
    std::mutex order_mutex;

    // Subscribe with different priorities
    StaticEventManager<DefaultMutex>::subscribe<Event3>(
        [&](const Event3&) {
            std::lock_guard lock(order_mutex);
            call_order.push_back(2);
        },
        -100
    );

    StaticEventManager<DefaultMutex>::subscribe<Event3>(
        [&](const Event3&) {
            std::lock_guard lock(order_mutex);
            call_order.push_back(0);
        },
        100
    );

    StaticEventManager<DefaultMutex>::subscribe<Event3>(
        [&](const Event3&) {
            std::lock_guard lock(order_mutex);
            call_order.push_back(1);
        },
        0
    );

    Event3 event(1);
    StaticEventManager<DefaultMutex>::publish(event);

    assert(call_order.size() == 3);
    assert(call_order[0] == 0);
    assert(call_order[1] == 1);
    assert(call_order[2] == 2);
    
    std::cout << "PASSED\n";
}

// Test 4: Member function subscription
void test_member_function_subscription() {
    std::cout << "Test 4: Member function subscription... ";
    
    auto subscriber = std::make_shared<TestSubscriber>();
    
    StaticEventManager<DefaultMutex>::subscribe<Event4>(
        subscriber,
        &TestSubscriber::on_event,
        100
    );

    Event4 event(999);
    StaticEventManager<DefaultMutex>::publish(event);

    assert(subscriber->call_count == 1);
    assert(subscriber->last_value == 999);
    
    std::cout << "PASSED\n";
}

// Test 5: Weak pointer cleanup
void test_weak_pointer_cleanup() {
    std::cout << "Test 5: Weak pointer cleanup... ";
    
    {
        auto subscriber = std::make_shared<TestSubscriber>();
        StaticEventManager<DefaultMutex>::subscribe<Event4>(
            subscriber,
            &TestSubscriber::on_event
        );
        
        Event4 event(111);
        StaticEventManager<DefaultMutex>::publish(event);
        assert(subscriber->call_count == 1);
    }
    // subscriber is destroyed here

    // Publish again - should trigger cleanup of expired weak_ptr
    Event4 event(222);
    StaticEventManager<DefaultMutex>::publish(event);
    
    // If we get here without crash, cleanup worked
    std::cout << "PASSED\n";
}

// Test 6: Cancellable event
void test_cancellable_event() {
    std::cout << "Test 6: Cancellable event... ";
    
    int first_callback_count = 0;
    int second_callback_count = 0;

    StaticEventManager<DefaultMutex>::subscribe<Event6>(
        [&](const Event6& event) {
            first_callback_count++;
            const_cast<Event6&>(event).cancel();
        },
        100
    );

    StaticEventManager<DefaultMutex>::subscribe<Event6>(
        [&](const Event6&) {
            second_callback_count++;
        },
        -100
    );

    Event6 event(42);
    StaticEventManager<DefaultMutex>::publish(event);

    assert(first_callback_count == 1);
    assert(second_callback_count == 0);  // Should not be called due to cancellation
    assert(event.is_cancelled());
    
    std::cout << "PASSED\n";
}

// Test 7: Thread safety
void test_thread_safety() {
    std::cout << "Test 7: Thread safety... ";
    
    std::atomic<int> total_calls{0};

    auto subscribe_thread = [&]() {
        for (int i = 0; i < 100; i++) {
            StaticEventManager<DefaultMutex>::subscribe<Event7>(
                [&](const Event7&) {
                    total_calls++;
                }
            );
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    };

    auto publish_thread = [&]() {
        for (int i = 0; i < 100; i++) {
            Event7 event(i);
            StaticEventManager<DefaultMutex>::publish(event);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    };

    std::thread t1(subscribe_thread);
    std::thread t2(publish_thread);
    std::thread t3(publish_thread);

    t1.join();
    t2.join();
    t3.join();

    // If we get here without crash, thread safety is working
    assert(total_calls > 0);
    
    std::cout << "PASSED\n";
}

// Test 8: Recursive publish
void test_recursive_publish() {
    std::cout << "Test 8: Recursive publish... ";
    
    std::atomic<int> call_count{0};
    const int max_depth = 5;

    StaticEventManager<DefaultMutex>::subscribe<Event8>(
        [&](const Event8& event) {
            call_count++;
            if (event.depth < max_depth) {
                Event8 new_event(event.depth + 1);
                StaticEventManager<DefaultMutex>::publish(new_event);
            }
        }
    );

    Event8 event(0);
    StaticEventManager<DefaultMutex>::publish(event);

    // Should be called max_depth + 1 times (0, 1, 2, 3, 4, 5)
    assert(call_count == max_depth + 1);
    
    std::cout << "PASSED\n";
}

// Test 9: Subscribe from callback
void test_subscribe_from_callback() {
    std::cout << "Test 9: Subscribe from callback... ";
    
    std::atomic<int> inner_callback_count{0};
    bool subscribed = false;

    StaticEventManager<DefaultMutex>::subscribe<Event9>(
        [&](const Event9&) {
            if (!subscribed) {
                StaticEventManager<DefaultMutex>::subscribe<Event9>(
                    [&](const Event9&) {
                        inner_callback_count++;
                    }
                );
                subscribed = true;
            }
        }
    );

    Event9 event1(1);
    StaticEventManager<DefaultMutex>::publish(event1);

    // First publish triggers subscription
    assert(inner_callback_count == 0);

    Event9 event2(2);
    StaticEventManager<DefaultMutex>::publish(event2);

    // Second publish should trigger the newly subscribed callback
    assert(inner_callback_count == 1);
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "Running StaticEventBus tests...\n\n";

    try {
        test_basic_subscription();
        test_multiple_subscribers();
        test_priority_ordering();
        test_member_function_subscription();
        test_weak_pointer_cleanup();
        test_cancellable_event();
        test_thread_safety();
        test_recursive_publish();
        test_subscribe_from_callback();

        std::cout << "\n✓ All tests passed!\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception\n";
        return 1;
    }
}
