#ifndef URL_UTILS_H_
#define URL_UTILS_H_

#include <string>
#include <string_view>

namespace url_utils {

std::string Decode(std::string_view str);

}  // namespace url_utils

#endif  // URL_UTILS_H_