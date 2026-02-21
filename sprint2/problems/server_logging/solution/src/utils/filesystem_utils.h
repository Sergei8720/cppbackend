#pragma once

#include <filesystem>

namespace fs_utils {

namespace fs = std::filesystem;

bool IsSubPath(fs::path path, fs::path base);

}