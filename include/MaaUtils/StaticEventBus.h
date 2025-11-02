#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <typeindex>

#include "MaaUtils/Conf.h"
#include "MaaUtils/Port.h"

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

class EventStorageBase {
public:
    virtual ~EventStorageBase() = default;
};

template<IsEvent EventT>
struct EventStorage : public EventStorageBase {
    struct Subscription {
        int priority;
        bool has_owner;
        std::function<void(EventT&)> callback;
        std::weak_ptr<void> owner;

        bool operator<(const Subscription& other) const {
            // Higher priority subscriptions come first
            return priority > other.priority;
        }
    };

    std::vector<Subscription> subscriptions;
};

class EventStorageRegistry {
private:
    std::unordered_map<std::type_index, std::unique_ptr<EventStorageBase>> storages_;
public:
    template<IsEvent EventT>
    EventStorage<EventT>& get_storage() {
        std::type_index type_idx(typeid(EventT));
        auto it = storages_.find(type_idx);
        if (it == storages_.end()) {
            auto storage = std::make_unique<EventStorage<EventT>>();
            auto* storage_ptr = storage.get();
            storages_[type_idx] = std::move(storage);
            return *storage_ptr;
        } else {
            return *static_cast<EventStorage<EventT>*>(it->second.get());
        }
    }
};

MAA_UTILS_API EventStorageRegistry& get_event_storage_registry();

class StaticEventManager {
private:
    template<IsEvent EventT>
    inline static EventStorage<EventT>& get_event_storage() {
        static auto* cached_storage = &get_event_storage_registry().get_storage<EventT>();
        return *cached_storage;
    }

public:
    // Free function subscription
    template<IsEvent EventT, typename Callable>
    static void subscribe(Callable&& callback, int priority = 0) {
        auto& storage = get_event_storage<EventT>();
        
        auto wrapper = [callback = std::forward<Callable>(callback)](EventT& event) mutable {
            if constexpr (std::is_invocable_v<Callable&, EventT&>) {
                callback(event);
            } else if constexpr (std::is_invocable_v<Callable&, const EventT&>) {
                callback(event);
            } else {
                static_assert(false, "Callback must be invocable with EventT& or const EventT&");
            }
        };
        
        storage.subscriptions.insert(
            std::lower_bound(
                storage.subscriptions.begin(),
                storage.subscriptions.end(),
                typename EventStorage<EventT>::Subscription{priority, false, {}, {}}
            ),
            {priority, false, std::move(wrapper), {}}
        );
    }

    // Member function subscription
    template<IsEvent EventT, typename T, typename Callable>
    static void subscribe(
        const std::shared_ptr<T>& owner,
        Callable&& callback,
        int priority = 0
    ) {
        auto weak_owner = std::weak_ptr<T>(owner);
        auto wrapper = [weak_owner, callback = std::forward<Callable>(callback)](EventT& event) mutable {
            if (auto shared_owner = weak_owner.lock()) {
                if constexpr (std::is_invocable_v<Callable&, EventT&>) {
                    callback(event);
                } else if constexpr (std::is_invocable_v<Callable&, const EventT&>) {
                    callback(event);
                } else if constexpr (std::is_invocable_v<Callable&, T*, EventT&>) {
                    (shared_owner.get()->*callback)(event);
                } else if constexpr (std::is_invocable_v<Callable&, T*, const EventT&>) {
                    (shared_owner.get()->*callback)(event);
                } else {
                    static_assert(false, "Callback must be invocable with EventT& or const EventT&");
                }
            }
        };
        auto& storage = get_event_storage<EventT>();
        storage.subscriptions.insert(
            std::lower_bound(
                storage.subscriptions.begin(),
                storage.subscriptions.end(),
                typename EventStorage<EventT>::Subscription{priority, true, {}, {}}
            ),
            {priority, true, std::move(wrapper), weak_owner}
        );
    }

    template<IsEvent EventT>
    static void publish(EventT& event) {
        auto& storage = get_event_storage<EventT>();
        bool need_erase = false;
        for (const auto& subscription : storage.subscriptions) {
            if (subscription.has_owner && subscription.owner.expired()) {
                need_erase = true;
                continue;
            }
            subscription.callback(event);
            if constexpr (std::derived_from<EventT, CancellableEvent>) {
                if (event.is_cancelled()) {
                    break;
                }
            }
        }
        if (need_erase) {
            std::erase_if(
                storage.subscriptions,
                [](const auto& sub) {
                    return sub.has_owner && sub.owner.expired();
                }
            );
        }
    }

    StaticEventManager() = delete;
};

MAA_NS_END
