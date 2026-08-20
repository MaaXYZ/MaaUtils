#pragma once

#include <filesystem>

#include "MaaUtils/Conf.h"
#include "MaaUtils/Port.h"

MAA_NS_BEGIN

MAA_UTILS_API const std::filesystem::path& library_dir();
MAA_UTILS_API std::filesystem::path get_library_path(void* addr);

MAA_NS_END
