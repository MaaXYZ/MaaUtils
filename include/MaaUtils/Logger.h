#pragma once

#include "MaaUtils/LoggerUtils.h"
#include "MaaUtils/ScopeLeave.hpp"

MAA_LOG_NS_BEGIN

class MAA_UTILS_API Logger
{
public:
    static constexpr std::string_view kLogFilename = "maafw.log";
    static constexpr std::string_view kLogbakFilename = "maafw.bak.{}.log";

    // perf trace 通道（独立文件，独立锁，无 Logger 默认前缀，纯 CSV 行）
    static constexpr std::string_view kPerfTraceSubdir = "perf";
    static constexpr std::string_view kPerfTraceFilenameFormat = "maafw_perf_trace_{}.csv";
    static constexpr std::string_view kPerfTraceCsvHeader = "ts_us,tid,scope,name,extra,elapsed_us";

public:
    static Logger& get_instance();

    ~Logger() { close(); }

    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;

    template <typename... args_t>
    auto fatal(args_t&&... args)
    {
        return stream(level::fatal, std::forward<args_t>(args)...);
    }

    template <typename... args_t>
    auto error(args_t&&... args)
    {
        return stream(level::error, std::forward<args_t>(args)...);
    }

    template <typename... args_t>
    auto warn(args_t&&... args)
    {
        return stream(level::warn, std::forward<args_t>(args)...);
    }

    template <typename... args_t>
    auto info(args_t&&... args)
    {
        return stream(level::info, std::forward<args_t>(args)...);
    }

    template <typename... args_t>
    auto debug(args_t&&... args)
    {
        return stream(level::debug, std::forward<args_t>(args)...);
    }

    template <typename... args_t>
    auto trace(args_t&&... args)
    {
        return stream(level::trace, std::forward<args_t>(args)...);
    }

    // perf trace 通道入口。返回的 PerfLogStream 不带任何 Logger 默认前缀，
    // 调用方负责把整行 CSV 文本拼好。析构时一次性原子追加一行到 perf 文件。
    // perf 文件随 start_logging() 打开；若日志目录未设置，当前写入会静默丢弃。
    PerfLogStream perf_stream();

    void start_logging(std::filesystem::path dir);
    void set_stdout_level(level lv);
    void flush();

private:
    template <typename... args_t>
    LogStream stream(level lv, args_t&&... args)
    {
        count_and_check_flush();

        bool std_out = lv <= stdout_level_;
        return LogStream(trace_mutex_, ofs_, lv, std_out, std::forward<args_t>(args)...);
    }

private:
    Logger() = default;

    void reinit();
    void cleanup();
    bool rotate();
    void open(bool append = true);
    void close();
    void log_proc_info();
    void count_and_check_flush();

    // perf 通道相关（实现见 Logger.cpp）
    void perf_reset_locked();
    void perf_try_open_locked();
    void perf_close_locked();

    LogStream internal_dbg();

private:
    std::filesystem::path log_dir_;
    std::filesystem::path log_path_;

#ifdef MAA_DEBUG
    level stdout_level_ = level::all;
#else
    level stdout_level_ = level::error;
#endif
    std::ofstream ofs_;
    std::mutex trace_mutex_;

    size_t log_count_ = 0;

    // perf 通道：与主日志完全分离
    std::mutex perf_init_mutex_; // 保护 perf 初始化状态及 log_dir_ 切换
    std::mutex perf_mutex_;      // 保护对 perf_ofs_ 的实际写入
    std::ofstream perf_ofs_;
    bool perf_initialized_ = false;
};

class LogScopeEnterHelper
{
public:
    template <typename... args_t>
    explicit LogScopeEnterHelper(args_t&&... args)
        : stream_(Logger::get_instance().debug(std::forward<args_t>(args)...))
    {
    }

    ~LogScopeEnterHelper() { stream_ << "| enter"; }

    LogStream& operator()() { return stream_; }

private:
    LogStream stream_;
};

template <typename... args_t>
class LogScopeLeaveHelper
{
public:
    explicit LogScopeLeaveHelper(args_t&&... args)
        : args_(std::forward<args_t>(args)...)
    {
    }

    ~LogScopeLeaveHelper()
    {
        std::apply([](auto&&... args) { return Logger::get_instance().trace(std::forward<decltype(args)>(args)...); }, std::move(args_))
            << "| leave," << duration_since(start_);
    }

private:
    std::tuple<args_t...> args_;
    std::chrono::time_point<std::chrono::steady_clock> start_ = std::chrono::steady_clock::now();
};

inline constexpr std::string_view pertty_file(std::string_view file)
{
    size_t pos = file.find_last_of(std::filesystem::path::preferred_separator);
    return file.substr(pos + 1, file.size());
}

MAA_LOG_NS_END

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#define MAA_FILE MAA_LOG_NS::pertty_file(__FILE__)
#define MAA_LINE std::string_view("L" LINE_STRING)
#ifdef _MSC_VER
#define MAA_FUNCTION std::string_view(__FUNCTION__)
#else
#define MAA_FUNCTION std::string_view(__PRETTY_FUNCTION__)
#endif
#define LOG_ARGS MAA_FILE, MAA_LINE, MAA_FUNCTION

#define LogFatal MAA_LOG_NS::Logger::get_instance().fatal(LOG_ARGS)
#define LogError MAA_LOG_NS::Logger::get_instance().error(LOG_ARGS)
#define LogWarn MAA_LOG_NS::Logger::get_instance().warn(LOG_ARGS)
#define LogInfo MAA_LOG_NS::Logger::get_instance().info(LOG_ARGS)
#define LogDebug MAA_LOG_NS::Logger::get_instance().debug(LOG_ARGS)
#define LogTrace MAA_LOG_NS::Logger::get_instance().trace(LOG_ARGS)

// perf 通道入口。无任何前缀，调用方拼好整行 CSV 文本即可。
// 此宏在所有构建中都存在，供 MaaFramework 的 perf trace 功能按需调用。
#define LogPerf MAA_LOG_NS::Logger::get_instance().perf_stream()

#define LogFunc                                                   \
    MAA_LOG_NS::LogScopeLeaveHelper ScopeHelperVarName(LOG_ARGS); \
    MAA_LOG_NS::LogScopeEnterHelper(LOG_ARGS)()

#define VAR_RAW(x) "[" << #x << "=" << (x) << "] "
#define VAR(x) MAA_LOG_NS::separator::none << VAR_RAW(x) << MAA_LOG_NS::separator::space
#define VAR_VOIDP_RAW(x) "[" << #x << "=" << reinterpret_cast<void*>(x) << "] "
#define VAR_VOIDP(x) MAA_LOG_NS::separator::none << VAR_VOIDP_RAW(x) << MAA_LOG_NS::separator::space
