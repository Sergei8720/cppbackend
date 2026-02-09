#include "filesystem_utils.h"

#include <algorithm>

namespace fs_utils {

namespace fs = std::filesystem;

bool IsSubPath(fs::path path, fs::path base) {
  if (path.empty() || base.empty()) {
    return false;
  }

  path = fs::weakly_canonical(path);
  base = fs::weakly_canonical(base);

  auto path_iter = path.begin();
  auto base_iter = base.begin();

  while (base_iter != base.end()) {
    if (path_iter == path.end()) {
      return false;
    }
    
    if (*path_iter != *base_iter) {
      return false;
    }
    
    ++path_iter;
    ++base_iter;
  }

  return true;
}

fs::path GetCanonicalPath(const fs::path& path) {
  try {
    return fs::weakly_canonical(path);
  } catch (...) {
    return path;
  }
}

bool IsPathWithinRoot(const fs::path& path, const fs::path& root) {
  fs::path canonical_path = GetCanonicalPath(path);
  fs::path canonical_root = GetCanonicalPath(root);
  
  return IsSubPath(canonical_path, canonical_root);
}

fs::path AppendPathSafely(const fs::path& base, const fs::path& relative) {
  fs::path result = GetCanonicalPath(base / relative);
  fs::path canonical_base = GetCanonicalPath(base);
  
  if (!IsSubPath(result, canonical_base)) {
    throw std::runtime_error("Path traversal attempt detected");
  }
  
  return result;
}

}  // namespace fs_utils