#include "api_v1_response_storage.h"

namespace response_storage {

std::vector<std::string_view> SplitUrl(std::string_view str) {
  std::vector<std::string_view> result;
  const std::string delim = "/";
  
  if (str.empty() || str == delim) {
    return result;
  }
  
  size_t start = 1U;
  size_t end = str.find(delim, start);
  
  while (end != std::string::npos) {
    result.push_back(str.substr(start, end - start));
    start = end + delim.length();
    end = str.find(delim, start);
  }
  
  result.push_back(str.substr(start, end));
  return result;
}

}  // namespace response_storage