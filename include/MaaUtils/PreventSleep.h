#pragma once

#include <memory>
#include <string_view>

#include "MaaUtils/Conf.h"
#include "MaaUtils/NonCopyable.hpp"
#include "MaaUtils/Port.h"

MAA_NS_BEGIN

// Prevents the system from idle sleeping and turning off the display while alive.
// Does not prevent the user from locking the screen manually.
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
