#include "request_handlers_utility.h"
#include <string>

namespace rh_storage {

std::vector<std::string_view> SplitUrl(std::string_view str) {
    std::vector<std::string_view> result;
    std::string delim = "/";
    
    if (str.empty() || str == delim) {
        return result;
    }
    
    size_t start = 0;
    if (str[0] == '/') {
        start = 1;
    }
    
    size_t end = str.find(delim, start);
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delim.length();
        end = str.find(delim, start);
    }
    
    if (start < str.length()) {
        result.push_back(str.substr(start));
    }
    
    return result;
}

}