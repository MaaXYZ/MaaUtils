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

class MAA_UTILS_API LogStream
{
public:
    template <typename... args_t>
    LogStream(std::mutex& m, std::ofstream& s, level lv, bool std_out,
              size_t max_log_size = kDefaultMaxLogSize,
              size_t truncated_size = kDefaultTruncatedSize,
              args_t&&... args)
        : mutex_(m)
        , stream_(s)
        , lv_(lv)
        , stdout_(std_out)
        , max_log_size_(max_log_size)
        , truncated_size_(truncated_size)
    {
        stream_props(std::forward<args_t>(args)...);
    }

    LogStream(const LogStream&) = delete;
    LogStream(LogStream&&) noexcept = default;

    ~LogStream()
    {
        std::unique_lock lock(mutex_);

        // Materialize buffer content once and reuse for all outputs
        auto content = buffer_.str();

        // Output to stdout (skip if too large to avoid flooding console)
        if (stdout_) {
            if (content.size() > max_log_size_) {
                std::cerr << "[WARNING: Log entry too large (" << content.size()
                         << " bytes), skipping stdout output]" << std::endl;
            } else {
                std::cout << stdout_string(content) << std::endl;
            }
        }

        // Truncate if content exceeds size limit
        if (content.size() > max_log_size_) {
            const size_t original_size = content.size();
            content = content.substr(0, truncated_size_) +
                      " ... [TRUNCATED: " + std::to_string(original_size) +
                      " bytes total, likely a bug - check for large objects passed to logger]";
        }

        // Write to file (check if stream is valid and open)
        if (!stream_.is_open()) {
            std::cerr << "[ERROR: Log stream is not open, log entry lost]" << std::endl;
        } else if (!stream_.good()) {
            std::cerr << "[ERROR: Log stream is in error state, log entry lost]" << std::endl;
        } else {
            stream_ << content << std::endl;
        }

        // Clear buffer to prevent content accumulation
        buffer_.str("");
        buffer_.clear();
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
        json::value j(std::forward<T>(value));
        // 直接 dumps 的 string 会多一对双引号，有点难看
        buffer_ << (j.is_string() ? j.as_string() : j.dumps()) << sep.str;
    }

    template <typename... args_t>
    void stream_props(args_t&&... args)
    {
#ifdef _WIN32
        int pid = _getpid();
#else
        int pid = ::getpid();
#endif
        auto tid = static_cast<uint16_t>(std::hash<std::thread::id> {}(std::this_thread::get_id()));

        std::string props = std::format("[{}][{}][Px{}][Tx{}]", format_now(), level_str(), pid, tid);
        for (auto&& arg : { args... }) {
            props += std::format("[{}]", arg);
        }
        stream(props, sep_);
    }

    std::string stdout_string(const std::string& content);
    std::string_view level_str();

private:
    // Default log size limits (can be overridden via constructor parameters)
    static constexpr size_t kDefaultMaxLogSize = 1024 * 1024; // 1MB - triggers truncation
    static constexpr size_t kDefaultTruncatedSize = 1024; // 1KB - size after truncation
    static_assert(kDefaultTruncatedSize <= kDefaultMaxLogSize, "kDefaultTruncatedSize must be <= kDefaultMaxLogSize");

    std::mutex& mutex_;
    std::ofstream& stream_;
    const level lv_ = level::fatal;
    const bool stdout_ = false;
    const size_t max_log_size_;
    const size_t truncated_size_;

    separator sep_ = separator::space;
    std::stringstream buffer_;
};

MAA_LOG_NS_END
