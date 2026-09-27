#pragma once

#include <memory>
#include <string_view>

#include "MaaUtils/Conf.h"
#include "MaaUtils/NonCopyable.hpp"
#include "MaaUtils/Port.h"

MAA_NS_BEGIN

// Prevents the system from idle sleeping and turning off the display while alive.
// Implemented on Windows and macOS. Other platforms log a warning and do nothing.
// Does not prevent the user from locking the screen manually.
// Based on OBS os_inhibit_sleep_*:
// https://github.com/obsproject/obs-studio/blob/50530ce9046599e698c5d2068e4f053fae2318f6/libobs/util/platform.h#L172
class MAA_UTILS_API PreventSleep : public NonCopyable
{
public:
    explicit PreventSleep(std::string_view reason);
    ~PreventSleep();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

MAA_NS_END
