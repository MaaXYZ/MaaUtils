#define _CRT_SECURE_NO_WARNINGS

#include "MaaUtils/Logger.h"

#ifdef _WIN32
#include "MaaUtils/SafeWindows.hpp"

#include <io.h>
#include <sysinfoapi.h>
#else
#include <sys/utsname.h>
#endif

#include "MaaUtils/Encoding.h"
#include "MaaUtils/ImageIo.h"
#include "MaaUtils/Platform.h"
#include "MaaUtils/Uuid.h"

#pragma message("MaaUtils MAA_VERSION: " MAA_VERSION)

MAA_LOG_NS_BEGIN

constexpr separator separator::none("");
constexpr separator separator::space(" ");
constexpr separator separator::tab("\t");
constexpr separator separator::newline("\n");
constexpr separator separator::comma(",");

static constexpr std::string_view kSplitLine = "-----------------------------";

std::string LogStream::stdout_string()
{
    std::string color;

    switch (lv_) {
    case level::fatal:
    case level::error:
        color = "\033[31m";
        break;
    case level::warn:
        color = "\033[33m";
        break;
    case level::info:
        color = "\033[32m";
        break;
    case level::debug:
    case level::trace:
        break;
    default:
        break;
    }

    return color + utf8_to_crt(buffer_.str()) + "\033[0m";
}

std::string_view LogStream::level_str()
{
    switch (lv_) {
    case level::fatal:
        return "FTL";
    case level::error:
        return "ERR";
    case level::warn:
        return "WRN";
    case level::info:
        return "INF";
    case level::debug:
        return "DBG";
    case level::trace:
        return "TRC";
    default:
        break;
    }
    return "NoLV";
}

Logger& Logger::get_instance()
{
    static Logger unique_instance;
    return unique_instance;
}

void Logger::start_logging(std::filesystem::path dir)
{
    log_dir_ = std::move(dir);
    if (log_dir_.empty()) {
        log_path_.clear();
    }
    else {
        log_path_ = log_dir_ / kLogFilename;
    }
    reinit();
}

void Logger::set_stdout_level(level lv)
{
    stdout_level_ = lv;
}

void Logger::flush()
{
    bool rotated = rotate();
    open(!rotated);

    if (rotated) {
        log_proc_info();
    }
}

void Logger::reinit()
{
    bool rotated = rotate();
    open(!rotated);
    log_proc_info();
}

bool Logger::rotate()
{
    if (log_path_.empty() || !std::filesystem::exists(log_path_)) {
        return false;
    }

    std::unique_lock trace_lock(trace_mutex_);
    if (ofs_.is_open()) {
        ofs_.close();
    }

    constexpr uintmax_t MaxLogSize = 16ULL * 1024 * 1024;
    const uintmax_t log_size = std::filesystem::file_size(log_path_);
    if (log_size < MaxLogSize) {
        return false;
    }

    std::error_code ec;

    const std::filesystem::path bak_path = log_dir_ / kLogbakFilename;
    std::filesystem::copy(log_path_, bak_path, std::filesystem::copy_options::overwrite_existing, ec);

    const std::filesystem::path dumps_bak_path = log_dir_ / kDumpsbakDirname;
    if (std::filesystem::exists(dumps_bak_path)) {
        std::filesystem::remove_all(dumps_bak_path, ec);
    }
    return true;
}

void Logger::open(bool append)
{
    if (log_path_.empty()) {
        return;
    }
    std::filesystem::create_directories(log_dir_);

    std::unique_lock trace_lock(trace_mutex_);
    if (ofs_.is_open()) {
        ofs_.close();
    }

#ifdef _WIN32

    // https://stackoverflow.com/questions/55513974/controlling-inheritability-of-file-handles-created-by-c-stdfstream-in-window
    std::string str_log_path = log_path_.string();
    FILE* file_ptr = fopen(str_log_path.c_str(), append ? "a" : "w");
    SetHandleInformation((HANDLE)_get_osfhandle(_fileno(file_ptr)), HANDLE_FLAG_INHERIT, 0);
    ofs_ = std::ofstream(file_ptr);

#else

    ofs_ = std::ofstream(log_path_, std::ios::out | (append ? std::ios::app : std::ios::trunc));

#endif
}

void Logger::close()
{
    internal_dbg() << kSplitLine;
    internal_dbg() << "Close log";
    internal_dbg() << kSplitLine;

    std::unique_lock trace_lock(trace_mutex_);
    if (ofs_.is_open()) {
        ofs_.close();
    }
}

static std::string sys_info()
{
#ifdef _WIN32
    SYSTEM_INFO sys {};
    GetNativeSystemInfo(&sys);

    // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info
    std::string arch;
    switch (sys.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        arch = "x64";
        break;
    case PROCESSOR_ARCHITECTURE_ARM64:
        arch = "ARM64";
        break;
    default:
        arch = "arch" + std::to_string(sys.wProcessorArchitecture);
        break;
    }
    return std::format("{} {}", "Windows", arch);
#else
    utsname uts {};
    uname(&uts);
    return std::format("{} {}", (const char*)uts.sysname, (const char*)uts.machine);
#endif
}

void Logger::log_proc_info()
{
    internal_dbg() << kSplitLine;
    internal_dbg() << "MAA Process Start";
    internal_dbg() << "Version" << MAA_VERSION;
    internal_dbg() << "Built at" << __DATE__ << __TIME__;
    internal_dbg() << sys_info();
    internal_dbg() << "Working" << std::filesystem::current_path();
    internal_dbg() << "Logging" << log_path_;
    internal_dbg() << kSplitLine;
}

void Logger::count_and_check_flush()
{
    static constexpr size_t kMaxCount = 1'000'000;

    if (++log_count_ < kMaxCount) {
        return;
    }

    log_count_ = 0;

    internal_dbg() << kSplitLine;
    internal_dbg() << "Too many logs, flushing...";
    internal_dbg() << kSplitLine;

    flush();
}

LogStream Logger::internal_dbg()
{
    return debug("Logger");
}

MAA_LOG_NS_END
