#pragma once
#include <filesystem>

namespace fs_utils {

bool IsSubPath(std::filesystem::path path, std::filesystem::path base);

}  // namespace fs_utils