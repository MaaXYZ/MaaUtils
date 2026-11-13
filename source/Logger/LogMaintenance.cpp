#include "LogMaintenance.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <vector>

#include <zlib.h>

#include "MaaUtils/Platform.h"
#include "MaaUtils/ScopeLeave.hpp"
#include "MaaUtils/Time.hpp"

MAA_LOG_NS_BEGIN

namespace
{

enum class ArtifactKind
{
    Vision,
    Log,
    Error,
};

struct LogArtifact
{
    std::filesystem::path path;
    std::filesystem::file_time_type modified;
    uintmax_t size = 0;
    ArtifactKind kind = ArtifactKind::Log;
};

bool compress_log(LogArtifact& artifact)
{
    auto archive = artifact.path;
    archive += ".gz";
    auto temporary = archive;
    temporary += "." + format_now_for_filename() + ".tmp";

    std::error_code ec;
    if (std::filesystem::exists(archive, ec) || ec) {
        return false;
    }
    std::ifstream input(artifact.path, std::ios::binary);
    if (!input) {
        return false;
    }
#ifdef _WIN32
    gzFile output = gzopen_w(temporary.c_str(), "wb1x");
#else
    gzFile output = gzopen(temporary.c_str(), "wb1x");
#endif
    if (!output) {
        return false;
    }
    OnScopeLeave([&] {
        if (output) {
            gzclose(output);
        }
        std::filesystem::remove(temporary, ec);
    });

    std::array<char, 64 * 1024> buffer;
    while (input) {
        input.read(buffer.data(), buffer.size());
        auto size = static_cast<unsigned int>(input.gcount());
        if (size && gzwrite(output, buffer.data(), size) != static_cast<int>(size)) {
            return false;
        }
    }
    if (!input.eof()) {
        return false;
    }
    input.close();
    int result = gzclose(output);
    output = nullptr;
    if (result != Z_OK) {
        return false;
    }

    // Do not replace a backup which changed while it was being read.
    if (std::filesystem::file_size(artifact.path, ec) != artifact.size || ec
        || std::filesystem::last_write_time(artifact.path, ec) != artifact.modified || ec) {
        return false;
    }
    auto compressed_size = std::filesystem::file_size(temporary, ec);
    if (ec) {
        return false;
    }
    // Retention is based on the original log's age, not the compression time.
    std::filesystem::last_write_time(temporary, artifact.modified, ec);
    if (ec) {
        return false;
    }
    std::filesystem::rename(temporary, archive, ec);
    if (ec) {
        return false;
    }
    if (!std::filesystem::remove(artifact.path, ec) || ec) {
        return false;
    }
    artifact.path = std::move(archive);
    artifact.size = compressed_size;
    return true;
}

} // namespace

void maintain_log_files(const std::filesystem::path& dir)
{
    constexpr auto kMaxAge = std::chrono::hours(24 * 7);
    constexpr auto kVisionMaxAge = std::chrono::hours(24 * 3);
    constexpr uintmax_t kMaxBytes = 1024ULL * 1024 * 1024;
    const auto now = std::filesystem::file_time_type::clock::now();
    std::vector<LogArtifact> artifacts;

    std::error_code ec;
    auto iter = std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec);
    const auto end = std::filesystem::recursive_directory_iterator();
    for (; !ec && iter != end; iter.increment(ec)) {
        const auto& entry = *iter;
        if (!std::filesystem::is_regular_file(entry.symlink_status(ec)) || ec) {
            ec.clear();
            continue;
        }
        auto last_write = entry.last_write_time(ec);
        if (ec) {
            ec.clear();
            continue;
        }
        auto relative = entry.path().lexically_relative(dir);
        const auto name = path_to_utf8_string(relative.filename());
        const auto ext = relative.extension();
        const bool backup = relative.parent_path().empty() && name.starts_with("maafw.bak.");
        ArtifactKind kind;
        if (backup && (name.ends_with(".log") || name.ends_with(".log.gz"))) {
            kind = ArtifactKind::Log;
        }
        else if ((ext == ".png" || ext == ".jpg") && *relative.begin() == "vision") {
            kind = ArtifactKind::Vision;
        }
        else if ((ext == ".png" || ext == ".jpg") && *relative.begin() == "on_error") {
            kind = ArtifactKind::Error;
        }
        else {
            if (backup && name.ends_with(".tmp") && now - last_write > kMaxAge) {
                std::filesystem::remove(entry.path(), ec);
            }
            ec.clear();
            continue;
        }
        auto max_age = kind == ArtifactKind::Vision ? kVisionMaxAge : kMaxAge;
        if (now - last_write > max_age && std::filesystem::remove(entry.path(), ec)) {
            continue;
        }
        ec.clear();
        auto size = entry.file_size(ec);
        if (!ec) {
            artifacts.push_back({ entry.path(), last_write, size, kind });
        }
        ec.clear();
    }

    uintmax_t total_size = 0;
    for (auto& artifact : artifacts) {
        if (artifact.kind == ArtifactKind::Log && artifact.path.extension() == ".log") {
            compress_log(artifact);
        }
        total_size += artifact.size;
    }
    // Discard vision drawings first; retain failure screenshots for as long as possible.
    std::ranges::sort(artifacts, [](const LogArtifact& lhs, const LogArtifact& rhs) {
        return lhs.kind != rhs.kind ? lhs.kind < rhs.kind : lhs.modified < rhs.modified;
    });
    for (const auto& artifact : artifacts) {
        if (total_size <= kMaxBytes) {
            break;
        }
        if (std::filesystem::remove(artifact.path, ec)) {
            total_size -= artifact.size;
        }
    }
}

MAA_LOG_NS_END
