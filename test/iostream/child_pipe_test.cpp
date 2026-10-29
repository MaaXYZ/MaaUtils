#include "MaaUtils/IOStream/ChildPipeIOStream.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using MaaNS::ChildPipeIOStream;

static void require(bool value, const char* message)
{
    if (!value) {
        throw std::runtime_error(message);
    }
}

static std::string payload()
{
    std::string data(2 * 1024 * 1024 + 37, '\0');
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(i % 256);
    }
    return data;
}

static int child(const std::string& mode)
{
    if (mode == "binary") {
        std::cout << payload();
    }
    else if (mode == "partial" || mode == "delimiter_timeout") {
        std::cout << "partial" << std::flush;
        std::this_thread::sleep_for(2s);
    }
    else if (mode == "count") {
        std::cout << "abcdef" << std::flush;
        std::this_thread::sleep_for(2s);
    }
    else if (mode == "delimiter") {
        std::cout << "ready\ntail" << std::flush;
        std::this_thread::sleep_for(2s);
    }
    else if (mode == "long_delimiter") {
        std::cout << std::string(16 * 1024, 'x') << "\ntail";
    }
    else if (mode == "resume") {
        std::cout << "early" << std::flush;
        std::this_thread::sleep_for(300ms);
        std::cout << "late";
    }
    else if (mode == "continuous") {
        for (int i = 0; i < 100; ++i) {
            std::cout << 'x' << std::flush;
            std::this_thread::sleep_for(20ms);
        }
    }
    else if (mode == "nonzero") {
        std::cout << "failure";
        return 42;
    }
    else if (mode == "echo") {
        std::string line;
        std::getline(std::cin, line);
        std::cout << line << '\n';
    }
    else if (mode == "empty") {
        return 0;
    }
    else if (mode == "idle" || mode == "idle_until") {
        std::this_thread::sleep_for(300ms);
        std::cout << "ready\ntail";
    }
    else if (mode == "inherited_stdout") {
        pid_t pid = fork();
        if (pid < 0) {
            return 1;
        }
        if (pid == 0) {
            // An inherited writer must not prevent the reader's deadline.
            // The finite sleep also bounds cleanup if the unpatched test fails.
            std::this_thread::sleep_for(2s);
            _exit(0);
        }
    }
    else if (mode == "eof") {
        close(STDOUT_FILENO);
        std::this_thread::sleep_for(2s);
    }
    else if (mode == "silent" || mode == "interrupted" || mode == "no_read") {
        std::this_thread::sleep_for(2s);
    }
    else {
        return 1;
    }
    return 0;
}

static void run(const std::filesystem::path& executable, const std::string& mode)
{
    ChildPipeIOStream stream(executable, std::vector<std::string> { "--child", mode });
    if (mode == "echo") {
        require(stream.write("hello"), "writing to child stdin failed");
    }
    if (mode == "interrupted") {
        struct sigaction action { };
        action.sa_handler = [](int) {
        };
        sigemptyset(&action.sa_mask);
        require(sigaction(SIGALRM, &action, nullptr) == 0, "cannot install signal handler");
        itimerval timer { { 0, 5000 }, { 0, 5000 } };
        require(setitimer(ITIMER_REAL, &timer, nullptr) == 0, "cannot start signal timer");
    }
    const auto start = std::chrono::steady_clock::now();
    std::string result;
    if (mode == "count") {
        result = stream.read_some(3, 100ms);
        require(result == "abc", "read_some exceeded or lost requested bytes");
        require(stream.read_some(3, 100ms) == "def", "unread bytes were lost");
    }
    else if (mode == "delimiter") {
        result = stream.read_until("\n", 100ms);
        require(result == "ready\n", "delimiter was not preserved");
        require(stream.read_some(4, 100ms) == "tail", "read_until consumed too much");
    }
    else if (mode == "delimiter_timeout") {
        result = stream.read_until("\n", 100ms);
        require(result == "partial", "delimiter timeout lost partial data");
    }
    else if (mode == "long_delimiter") {
        result = stream.read_until("\n", 3s);
        require(result == std::string(16 * 1024, 'x') + "\n", "long delimiter read lost data");
        require(stream.read_some(4, 100ms) == "tail", "long delimiter read consumed the tail");
    }
    else if (mode == "idle") {
        result = stream.read();
        require(result == "ready\ntail" && stream.eof(), "unbounded read lost output or EOF");
    }
    else if (mode == "idle_until") {
        result = stream.read_until("\n");
        require(result == "ready\n", "unbounded delimiter read failed");
        require(stream.read() == "tail" && stream.eof(), "unbounded delimiter read consumed the tail");
    }
    else if (mode == "no_read") {
        require(stream.read_some(0).empty(), "zero-count read returned data");
        require(stream.read_until("").empty(), "empty delimiter read returned data");
        require(stream.read(0ms).empty(), "zero-timeout read returned data");
        require(stream.read(-1ms).empty(), "negative-timeout read returned data");
        require(stream.is_open() && !stream.eof(), "empty reads changed pipe state");
    }
    else {
        result = stream.read(mode == "binary" ? 3s : 100ms);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (mode == "interrupted") {
        itimerval timer { };
        require(setitimer(ITIMER_REAL, &timer, nullptr) == 0, "cannot stop signal timer");
    }
    std::cout << mode << ": read_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
              << " bytes=" << result.size() << std::endl;

    if (mode == "binary") {
        require(result == payload(), "binary output was truncated or changed");
        require(!stream.is_open(), "EOF was not detected");
        require(stream.eof(), "successful stdout completion was not detected");
        require(stream.release(), "successful child exit was lost");
    }
    else {
        // The 100ms timeout regressions must reject the original 2s block.
        // Long delimiter processing gets extra room for sanitizer overhead.
        require(elapsed < (mode == "long_delimiter" ? 3s : 700ms), "read ignored its deadline");
        if (mode == "silent" || mode == "eof" || mode == "inherited_stdout" || mode == "interrupted" || mode == "empty") {
            require(result.empty(), "unexpected output");
        }
        if (mode == "silent" || mode == "partial" || mode == "continuous" || mode == "delimiter_timeout" || mode == "inherited_stdout"
            || mode == "interrupted") {
            require(elapsed >= 80ms, "temporary lack of data was treated as EOF");
            require(stream.is_open(), "timeout incorrectly closed the readable stream");
            require(!stream.eof(), "timeout incorrectly reported stdout completion");
        }
        if (mode == "partial") {
            require(result == "partial", "timeout lost partial data");
        }
        if (mode == "continuous") {
            require(!result.empty() && result.size() < 100, "continuous writer bypassed deadline");
        }
        if (mode == "resume") {
            require(result == "early", "initial timed read lost data");
            require(stream.is_open(), "timeout marked stream EOF");
            require(stream.read(1s) == "late", "cannot read after timeout");
            require(stream.eof(), "resumed read did not reach EOF");
        }
        if (mode == "eof" || mode == "nonzero" || mode == "empty" || mode == "echo") {
            require(!stream.is_open(), "closed stdout was not detected");
            require(stream.eof(), "closed stdout was not reported as complete");
        }
        const auto cleanup_start = std::chrono::steady_clock::now();
        const bool success = stream.release();
        require(std::chrono::steady_clock::now() - cleanup_start < 700ms, "cleanup blocked");
        require(!stream.running(), "child survived release");
        if (mode == "nonzero") {
            require(result == "failure" && !success, "nonzero exit was swallowed");
        }
        if (mode == "silent" || mode == "partial" || mode == "interrupted" || mode == "continuous") {
            require(!success, "terminated child was reported as successful");
        }
        if (mode == "echo") {
            require(result == "hello\n" && success, "stdin/stdout round trip failed");
        }
        if (mode == "empty") {
            require(success, "empty successful exit was reported as a failure");
        }
        if (mode == "idle" || mode == "idle_until") {
            require(elapsed >= 250ms && success, "unbounded read did not wait for the delayed writer");
        }
    }
    // Direct children must have been reaped, including forced termination.
    int status = 0;
    require(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "child was not reaped");
}

int main(int argc, char** argv)
{
    if (argc == 3 && std::string(argv[1]) == "--child") {
        return child(argv[2]);
    }
    if (argc != 2) {
        return 2;
    }
    try {
        run(std::filesystem::absolute(argv[0]), argv[1]);
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
