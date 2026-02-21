#include "request_handlers_utility.h"

#include <string>

namespace rh_storage {

std::vector<std::string_view> SplitUrl(std::string_view str) {
  std::vector<std::string_view> result;
  std::string delimiter = "/";

  if (str.empty() || str == delimiter) {
    return result;
  }

  size_t start = 1U;
  size_t end = str.find(delimiter, start);

  while (end != std::string::npos) {
    result.push_back(str.substr(start, end - start));
    start = end + delimiter.length();
    end = str.find(delimiter, start);
  }

  result.push_back(str.substr(start, end));
  return result;
}

}