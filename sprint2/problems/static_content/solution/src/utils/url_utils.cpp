#include "url_utils.h"

#include <charconv>
#include <sstream>
#include <iomanip>

namespace url_utils {

std::string Decode(std::string_view str) {
  std::ostringstream decoded;
  
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '%') {
      if (i + 2 < str.size()) {
        int hex_value;
        auto result = std::from_chars(&str[i + 1], &str[i + 3], hex_value, 16);
        if (result.ec == std::errc()) {
          decoded << static_cast<char>(hex_value);
          i += 2;
        } else {
          decoded << str[i];
        }
      } else {
        decoded << str[i];
      }
    } else if (str[i] == '+') {
      decoded << ' ';
    } else {
      decoded << str[i];
    }
  }
  
  return decoded.str();
}

}  // namespace url_utils