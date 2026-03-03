#ifndef _WIN32
#include "MaaUtils/Runtime.h"

#include <dlfcn.h>

#ifdef __APPLE__
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
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
    const auto original_path_env = path_env;

    auto normalize = [](std::string s) -> std::string {
        while (s.size() > 1 && s.back() == '/') {
            s.pop_back();
        }
        return s;
    };

    std::unordered_set<std::string> existing_dirs;
    {
        std::string::size_type start = 0;
        while (start < path_env.size()) {
            auto pos = path_env.find(':', start);
            std::string seg;
            if (pos == std::string::npos) {
                seg = normalize(path_env.substr(start));
                if (!seg.empty()) {
                    existing_dirs.insert(std::move(seg));
                }
                break;
            }
            seg = normalize(path_env.substr(start, pos - start));
            if (!seg.empty()) {
                existing_dirs.insert(std::move(seg));
            }
            start = pos + 1;
        }
    }

    auto append = [&](const std::string& raw) {
        auto dir = normalize(raw);
        if (dir.empty() || existing_dirs.count(dir)) {
            return;
        }
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec) || ec) {
            return;
        }
        existing_dirs.insert(dir);
        if (path_env.empty()) {
            path_env = dir;
        }
        else {
            path_env += ':';
            path_env += dir;
        }
    };

    auto read_paths_from = [&](const std::filesystem::path& file) {
        std::ifstream ifs(file);
        if (!ifs.is_open()) {
            return;
        }
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            append(line);
        }
    };

    // read /etc/paths
    read_paths_from("/etc/paths");

    // read /etc/paths.d/*
    {
        std::error_code ec;
        std::filesystem::directory_iterator it("/etc/paths.d", ec);
        std::filesystem::directory_iterator end;
        for (; it != end; it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (it->is_regular_file(ec) && !ec) {
                read_paths_from(it->path());
            }
        }
    }

    // common paths that may not be in /etc/paths
    append("/opt/homebrew/bin");
    append("/opt/homebrew/sbin");
    append("/usr/local/bin");
    append("/usr/local/sbin");

    if (path_env != original_path_env) {
        setenv("PATH", path_env.c_str(), 1);
    }
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
