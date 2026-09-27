#ifdef __APPLE__

#include "MaaUtils/PreventSleep.h"

#include <string>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "MaaUtils/Logger.h"

MAA_NS_BEGIN

// Matches OBS os_inhibit_sleep_set_active:
// https://github.com/obsproject/obs-studio/blob/50530ce9046599e698c5d2068e4f053fae2318f6/libobs/util/platform-cocoa.m#L259
struct PreventSleep::Impl
{
    IOPMAssertionID assertion_id = kIOPMNullAssertionID;
};

PreventSleep::PreventSleep(std::string_view reason)
    : impl_(std::make_unique<Impl>())
{
    std::string reason_str(reason);
    CFStringRef cf_reason = CFStringCreateWithCString(kCFAllocatorDefault, reason_str.c_str(), kCFStringEncodingUTF8);
    if (!cf_reason) {
        cf_reason = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("MaaFramework"));
    }

    IOReturn ret = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, cf_reason, &impl_->assertion_id);
    CFRelease(cf_reason);

    if (ret != kIOReturnSuccess) {
        impl_->assertion_id = kIOPMNullAssertionID;
        LogWarn << "IOPMAssertionCreateWithName failed" << VAR(ret);
        return;
    }

    LogDebug << VAR(reason) << VAR(impl_->assertion_id);
}

PreventSleep::~PreventSleep()
{
    if (impl_->assertion_id == kIOPMNullAssertionID) {
        return;
    }

    IOReturn ret = IOPMAssertionRelease(impl_->assertion_id);
    if (ret != kIOReturnSuccess) {
        LogWarn << "IOPMAssertionRelease failed" << VAR(ret);
    }
}

MAA_NS_END

#endif // __APPLE__
