#include "MaaUtils/StaticEventBus.h"

MAA_NS_BEGIN

EventStorageRegistry& get_event_storage_registry() {
    static EventStorageRegistry registry;
    return registry;
}

MAA_NS_END