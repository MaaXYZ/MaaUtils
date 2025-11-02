#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <compare>
#include <vector>
#include <algorithm>
#include <optional>
#include <mutex>

#include "MaaUtils/Conf.h"

MAA_NS_BEGIN

class Event {
public:
    virtual ~Event() = default;
};

class CancellableEvent : public Event {
public:
    void cancel() { cancelled_ = true; }
    bool is_cancelled() const { return cancelled_; }

private:
    bool cancelled_ = false;
};

template<typename T>
concept IsEvent = std::derived_from<T, Event>;

class NoMutex {
public:
    void lock() noexcept {}
    void unlock() noexcept {}
};
class DefaultMutex {
private:
    std::mutex mutex_;
public:
    void lock() noexcept { mutex_.lock(); }
    void unlock() noexcept { mutex_.unlock(); }
};
template<typename T>
concept MutexType = std::same_as<T, NoMutex> || std::same_as<T, DefaultMutex>;

template<MutexType MutexT>
class StaticEventManager {
private:
    template<IsEvent EventT>
    struct Subscription {
        int priority;
        std::function<void(const EventT&)> callback;
        std::optional<std::weak_ptr<void>> owner;

        bool operator<(const Subscription& other) const {
            // Higher priority subscriptions come first
            return priority > other.priority;
        }
    };

    template<IsEvent EventT>
    struct EventStorage {
        std::vector<Subscription<EventT>> subscriptions;
        bool is_sorted = false;
        MutexT mutex;
    };
    template<IsEvent EventT>
    inline static EventStorage<EventT>& get_event_storage() {
        static EventStorage<EventT> storage;
        return storage;
    }

public:
    // Free function subscription
    template<IsEvent EventT>
    static void subscribe(
        std::function<void(const EventT&)> callback,
        int priority = 0
    ) {
        auto& storage = get_event_storage<EventT>();
        storage.mutex.lock();
        storage.subscriptions.push_back(Subscription<EventT>{priority, std::move(callback), std::nullopt});
        storage.is_sorted = false;
        storage.mutex.unlock();
    }

    // Member function subscription
    template<IsEvent EventT, typename T>
    static void subscribe(
        const std::shared_ptr<T>& owner,
        void (T::*member_func)(const EventT&),
        int priority = 0
    ) {
        auto weak_owner = std::weak_ptr<T>(owner);
        auto callback = [weak_owner, member_func](const EventT& event) {
            if (auto shared_owner = weak_owner.lock()) {
                (shared_owner.get()->*member_func)(event);
            }
        };
        auto& storage = get_event_storage<EventT>();
        storage.mutex.lock();
        storage.subscriptions.push_back(Subscription<EventT>{priority, std::move(callback), weak_owner});
        storage.is_sorted = false;
        storage.mutex.unlock();
    }

    template<IsEvent EventT>
    static void publish(EventT& event) {
        auto& storage = get_event_storage<EventT>();
        std::vector<Subscription<EventT>> subscriptions_copy;
        storage.mutex.lock();
        std::erase_if(
            storage.subscriptions,
            [](const Subscription<EventT>& sub) {
                return sub.owner.has_value() && sub.owner->expired();
            }
        );
        if (!storage.is_sorted) {
            std::sort(storage.subscriptions.begin(), storage.subscriptions.end());
            storage.is_sorted = true;
        }
        subscriptions_copy = storage.subscriptions;
        storage.mutex.unlock();

        for (const auto& subscription : subscriptions_copy) {
            subscription.callback(event);
            if constexpr (std::derived_from<EventT, CancellableEvent>) {
                if (event.is_cancelled()) {
                    break;
                }
            }
        }
    }

    StaticEventManager() = delete;
};

MAA_NS_END
