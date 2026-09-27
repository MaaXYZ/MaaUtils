#ifdef _WIN32

#include "MaaUtils/PreventSleep.h"
#include "MaaUtils/SafeWindows.hpp"

#include <string>

#include "MaaUtils/Encoding.h"
#include "MaaUtils/Logger.h"

MAA_NS_BEGIN

// OBS uses SetThreadExecutionState:
// https://github.com/obsproject/obs-studio/blob/50530ce9046599e698c5d2068e4f053fae2318f6/libobs/util/platform-windows.c#L1166
// PowerCreateRequest is used instead because SetThreadExecutionState is thread-scoped,
// so it can't be safely owned by an object that may be created and destroyed on different threads.
struct PreventSleep::Impl
{
    HANDLE request = nullptr;
    bool display_set = false;
    bool system_set = false;
};

PreventSleep::PreventSleep(std::string_view reason)
    : impl_(std::make_unique<Impl>())
{
    std::wstring wreason = to_u16(reason);

    REASON_CONTEXT context {};
    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = wreason.data();

    impl_->request = PowerCreateRequest(&context);
    if (impl_->request == INVALID_HANDLE_VALUE) {
        impl_->request = nullptr;
        LogWarn << "PowerCreateRequest failed" << VAR(GetLastError());
        return;
    }

    impl_->display_set = PowerSetRequest(impl_->request, PowerRequestDisplayRequired);
    if (!impl_->display_set) {
        LogWarn << "PowerSetRequest PowerRequestDisplayRequired failed" << VAR(GetLastError());
    }
    impl_->system_set = PowerSetRequest(impl_->request, PowerRequestSystemRequired);
    if (!impl_->system_set) {
        LogWarn << "PowerSetRequest PowerRequestSystemRequired failed" << VAR(GetLastError());
    }

    LogDebug << VAR(reason) << VAR(impl_->display_set) << VAR(impl_->system_set);
}

PreventSleep::~PreventSleep()
{
    if (!impl_->request) {
        return;
    }

    if (impl_->display_set) {
        PowerClearRequest(impl_->request, PowerRequestDisplayRequired);
    }
    if (impl_->system_set) {
        PowerClearRequest(impl_->request, PowerRequestSystemRequired);
    }
    CloseHandle(impl_->request);
}

MAA_NS_END

#endif // _WIN32
