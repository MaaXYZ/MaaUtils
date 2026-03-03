#ifndef _WIN32
#include "MaaUtils/Runtime.h"

#include <dlfcn.h>

#ifdef __APPLE__
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_set>
#endif

#include "MaaUtils/Platform.h"

#ifdef __APPLE__

__attribute__((constructor))
static void ensure_macos_path()
{
    std::string path_env;
    if (const char* existing = getenv("PATH")) {
        path_env = existing;
    }

    std::unordered_set<std::string> existing_dirs;
    {
        std::string::size_type start = 0;
        while (start < path_env.size()) {
            auto pos = path_env.find(':', start);
            if (pos == std::string::npos) {
                existing_dirs.insert(path_env.substr(start));
                break;
            }
            existing_dirs.insert(path_env.substr(start, pos - start));
            start = pos + 1;
        }
    }

    auto append = [&](const std::string& dir) {
        if (dir.empty() || existing_dirs.count(dir)) {
            return;
        }
        if (!std::filesystem::is_directory(dir)) {
            return;
        }
        existing_dirs.insert(dir);
        path_env += ':';
        path_env += dir;
    };

    // read /etc/paths
    if (std::ifstream ifs("/etc/paths"); ifs.is_open()) {
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            append(line);
        }
    }

    // read /etc/paths.d/*
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("/etc/paths.d", ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (std::ifstream ifs(entry.path()); ifs.is_open()) {
                std::string line;
                while (std::getline(ifs, line)) {
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    append(line);
                }
            }
        }
    }

    // common paths that may not be in /etc/paths
    append("/opt/homebrew/bin");
    append("/opt/homebrew/sbin");
    append("/usr/local/bin");
    append("/usr/local/sbin");

    setenv("PATH", path_env.c_str(), 1);
}

#endif // __APPLE__

MAA_NS_BEGIN

void init_library_dir();

static std::filesystem::path s_library_dir_cache;

const std::filesystem::path& library_dir()
{
    if (s_library_dir_cache.empty()) {
        init_library_dir();
    }

    return s_library_dir_cache;
}

void init_library_dir()
{
    Dl_info dl_info {};
    if (dladdr((void*)init_library_dir, &dl_info) == 0) {
        return;
    }

    s_library_dir_cache = MAA_NS::path(dl_info.dli_fname).parent_path();
}

MAA_NS_END

#endif
