#include "filesystem_utils.h"

namespace fs_utils {

using namespace std::literals;
namespace fs = std::filesystem;

bool IsSubPath(fs::path path, fs::path base) {
  path = fs::weakly_canonical(path);
  base = fs::weakly_canonical(base);

  auto base_iterator = base.begin();
  auto path_iterator = path.begin();

  for (; base_iterator != base.end(); ++base_iterator, ++path_iterator) {
    if ((path_iterator == path.end()) || (*path_iterator != *base_iterator)) {
      return false;
    }
  }

  return true;
}

}