#pragma once

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <type_traits>

#include "MaaUtils/Conf.h"
#include "MaaUtils/JsonExt.hpp"
#include "MaaUtils/Port.h"
#include "MaaUtils/Time.hpp"

namespace cv
{
class Mat;
}

MAA_LOG_NS_BEGIN

enum class level
{
    off = 0,
    fatal = 1,
    error = 2,
    warn = 3,
    info = 4,
    debug = 5,
    trace = 6,
    all = 7,
};

struct MAA_UTILS_API separator
{
    explicit constexpr separator(std::string_view s) noexcept
        : str(s)
    {
    }

    static const separator none;
    static const separator space;
    static const separator tab;
    static const separator newline;
    static const separator comma;

    std::string_view str;
};

template <typename T>
concept has_output_operator = requires { std::declval<std::ostringstream&>() << std::declval<T>(); };

class MAA_UTILS_API LogStream
{
public:
    template <typename... args_t>
    LogStream(std::mutex& m, std::ofstream& s, level lv, bool std_out, args_t&&... args)
        : mutex_(m)
        , stream_(s)
        , lv_(lv)
        , stdout_(std_out)
    {
        stream_props(std::forward<args_t>(args)...);
    }

    LogStream(const LogStream&) = delete;
    LogStream(LogStream&&) noexcept = default;

    ~LogStream()
    {
        std::unique_lock lock(mutex_);

        if (stdout_) {
            std::cout << stdout_string() << std::endl;
        }
        stream_ << std::move(buffer_).str() << std::endl;
    }

    template <typename T>
    LogStream& operator<<(T&& value)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, separator>) {
            sep_ = std::forward<T>(value);
        }
        else {
            stream(std::forward<T>(value), sep_);
        }

        return *this;
    }

    template <typename T>
    LogStream& operator,(T&& value)
    {
        stream(std::forward<T>(value), separator::none);
        return *this;
    }

private:
    template <typename T>
    void stream(T&& value, const separator& sep)
    {
        if constexpr (std::is_constructible_v<json::value, T>) {
            json::value j(std::forward<T>(value));
            // 直接 dumps 的 string 会多一对双引号，有点难看
            buffer_ << (j.is_string() ? j.as_string() : j.dumps()) << sep.str;
        }
        else if constexpr (std::is_constructible_v<json::array, T>) {
            json::array j(std::forward<T>(value));
            buffer_ << j.dumps() << sep.str;
        }
        else if constexpr (std::is_constructible_v<json::object, T>) {
            json::object j(std::forward<T>(value));
            buffer_ << j.dumps() << sep.str;
        }
        else if constexpr (has_output_operator<T>) {
            buffer_ << value << sep.str;
        }
        else {
            static_assert(false);
        }
    }

    template <typename... args_t>
    void stream_props(args_t&&... args)
    {
#ifdef _WIN32
        int pid = _getpid();
#else
        int pid = ::getpid();
#endif
        auto tid = static_cast<uint16_t>(std::hash<std::thread::id> { }(std::this_thread::get_id()));

        std::string props = std::format("[{}][{}][Px{}][Tx{}]", format_now(), level_str(), pid, tid);
        for (auto&& arg : { args... }) {
            props += std::format("[{}]", arg);
        }
        stream(props, sep_);
    }

    std::string stdout_string();
    std::string_view level_str();

private:
    std::mutex& mutex_;
    std::ofstream& stream_;
    const level lv_ = level::fatal;
    const bool stdout_ = false;

    separator sep_ = separator::space;
    std::stringstream buffer_;
};

MAA_LOG_NS_END
