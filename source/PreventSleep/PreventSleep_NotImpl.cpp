#if !defined(_WIN32) && !defined(__APPLE__) && !(defined(__linux__) && !defined(__ANDROID__))

#include "MaaUtils/PreventSleep.h"

#include "MaaUtils/Logger.h"

MAA_NS_BEGIN

struct PreventSleep::Impl
{
};

PreventSleep::PreventSleep(std::string_view reason)
    : impl_(std::make_unique<Impl>())
{
    LogWarn << "PreventSleep is not implemented on this platform" << VAR(reason);
}

PreventSleep::~PreventSleep() = default;

MAA_NS_END

#endif
