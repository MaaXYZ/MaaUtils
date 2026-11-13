#pragma once

#include <filesystem>

#include "MaaUtils/Conf.h"

MAA_LOG_NS_BEGIN

void maintain_log_files(const std::filesystem::path& dir);

MAA_LOG_NS_END
