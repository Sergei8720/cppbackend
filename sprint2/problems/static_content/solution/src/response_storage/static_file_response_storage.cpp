#include "static_file_response_storage.h"

#include <algorithm>
#include <cctype>

namespace response_storage {

fs::path GetStaticFilePath(const fs::path& static_content_root,
                           std::string_view target) {
  fs::path result_path = static_content_root;
  
  if (target.empty() || target == "/") {
    result_path /= kIndexFileName;
    return fs::weakly_canonical(result_path);
  }
  
  std::string decoded_path = url_utils::Decode(target.substr(1));
  fs::path rel_path(decoded_path);
  result_path = fs::weakly_canonical(result_path / rel_path);
  
  if (fs::is_directory(result_path)) {
    result_path /= kIndexFileName;
  }
  
  return fs::weakly_canonical(result_path);
}

std::string GetContentType(const fs::path& file_path) {
  std::string extension = file_path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  
  auto it = kExtensionToContentType.find(extension);
  if (it != kExtensionToContentType.end()) {
    return it->second;
  }
  
  return "application/octet-stream";
}

}  // namespace response_storage