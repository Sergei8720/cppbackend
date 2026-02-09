#ifndef FILESYSTEM_UTILS_H_
#define FILESYSTEM_UTILS_H_

#include <filesystem>
#include <string>

namespace fs_utils {

namespace fs = std::filesystem;

bool IsSubPath(fs::path path, fs::path base);

fs::path GetCanonicalPath(const fs::path& path);

bool IsPathWithinRoot(const fs::path& path, const fs::path& root);

fs::path AppendPathSafely(const fs::path& base, const fs::path& relative);

}  // namespace fs_utils

#endif  // FILESYSTEM_UTILS_H_