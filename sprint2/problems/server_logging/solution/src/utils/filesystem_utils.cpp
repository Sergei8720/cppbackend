#include "filesystem_utils.h"

namespace fs_utils {

bool IsSubPath(std::filesystem::path path, std::filesystem::path base) {
  path = std::filesystem::weakly_canonical(path);
  base = std::filesystem::weakly_canonical(base);
  
  auto path_it = path.begin();
  auto base_it = base.begin();
  
  while (path_it != path.end() && base_it != base.end()) {
    if (*path_it != *base_it) {
      return false;
    }
    ++path_it;
    ++base_it;
  }
  
  return base_it == base.end();
}

}  // namespace fs_utils