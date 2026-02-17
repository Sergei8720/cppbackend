#include "request_handlers_utility.h"

namespace rh_storage {

std::vector<std::string_view> SplitUrl(std::string_view str) {
  std::vector<std::string_view> result;
  
  if (str.empty() || str == "/") {
    return result;
  }
  
  const std::string delimiter = "/";
  size_t start = 1;
  size_t end = str.find(delimiter, start);
  
  while (end != std::string::npos) {
    result.push_back(str.substr(start, end - start));
    start = end + delimiter.length();
    end = str.find(delimiter, start);
  }
  
  result.push_back(str.substr(start));
  return result;
}

}  // namespace rh_storage