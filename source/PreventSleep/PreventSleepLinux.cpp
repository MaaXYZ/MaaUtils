#if defined(__linux__) && !defined(__ANDROID__)

#include "MaaUtils/PreventSleep.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "MaaUtils/IOStream/BoostIO.hpp"
#include "MaaUtils/Logger.h"

MAA_NS_BEGIN

struct PreventSleep::Impl
{
    // systemd-inhibit holds the lock while `cat` is alive; `cat` exits on stdin EOF,
    // so the lock is also released if this process crashes.
    boost::process::opstream inhibitor_stdin;
    boost::process::child inhibitor;

    std::thread screensaver_thread;
    std::mutex mutex;
    std::condition_variable cv;
    bool stop = false;

    bool start_inhibitor(std::string_view reason);
    bool start_screensaver_reset();
};

bool PreventSleep::Impl::start_inhibitor(std::string_view reason)
{
    auto exec = boost::process::search_path("systemd-inhibit");
    if (exec.empty()) {
        return false;
    }

    std::error_code ec;
    inhibitor = boost::process::child(
        exec,
        "--what=idle:sleep",
        "--who=MaaFramework",
        std::string("--why=") + std::string(reason),
        "--mode=block",
        "cat",
        boost::process::std_in<inhibitor_stdin, boost::process::std_out> boost::process::null,
        boost::process::std_err > boost::process::null,
        ec);
    if (ec) {
        LogWarn << "failed to start systemd-inhibit" << VAR(ec.message());
        return false;
    }

    LogDebug << "systemd-inhibit started" << VAR(inhibitor.id());
    return true;
}

bool PreventSleep::Impl::start_screensaver_reset()
{
    auto exec = boost::process::search_path("xdg-screensaver");
    if (exec.empty()) {
        return false;
    }

    screensaver_thread = std::thread([this, exec]() {
        using namespace std::chrono_literals;

        std::unique_lock lock(mutex);
        while (!stop) {
            lock.unlock();
            std::error_code ec;
            boost::process::system(
                exec,
                "reset",
                boost::process::std_out > boost::process::null,
                boost::process::std_err > boost::process::null,
                ec);
            if (ec) {
                LogWarn << "failed to run xdg-screensaver reset" << VAR(ec.message());
            }
            lock.lock();
            cv.wait_for(lock, 30s, [this]() { return stop; });
        }
    });

    LogDebug << "xdg-screensaver reset thread started";
    return true;
}

PreventSleep::PreventSleep(std::string_view reason)
    : impl_(std::make_unique<Impl>())
{
    if (impl_->start_inhibitor(reason)) {
        return;
    }
    if (impl_->start_screensaver_reset()) {
        return;
    }
    LogWarn << "neither systemd-inhibit nor xdg-screensaver is available, sleep is not prevented";
}

PreventSleep::~PreventSleep()
{
    if (impl_->inhibitor.valid()) {
        impl_->inhibitor_stdin.pipe().close();

        using namespace std::chrono_literals;
        std::error_code ec;
        auto start_time = std::chrono::steady_clock::now();
        while (impl_->inhibitor.running(ec) && std::chrono::steady_clock::now() - start_time < 500ms) {
            std::this_thread::sleep_for(10ms);
        }
        if (impl_->inhibitor.running(ec)) {
            impl_->inhibitor.terminate(ec);
        }
        impl_->inhibitor.wait(ec);
    }

    if (impl_->screensaver_thread.joinable()) {
        {
            std::unique_lock lock(impl_->mutex);
            impl_->stop = true;
        }
        impl_->cv.notify_all();
        impl_->screensaver_thread.join();
    }
}

MAA_NS_END

#endif // defined(__linux__) && !defined(__ANDROID__)
