#pragma once

// Standalone test shim: the production logger pulls in OpenCV. Do not mock I/O.
#include "MaaUtils/Time.hpp"
#include <thread>

struct TestLog
{
    template <typename T>
    TestLog& operator<<(const T&)
    {
        return *this;
    }
};

#define LogDebug TestLog()
#define LogError TestLog()
#define LogWarn TestLog()
#define VAR(value) (value)
