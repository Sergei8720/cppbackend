#include "request_handlers_utils.h"
#include <string>

namespace rh_storage{

const std::string BEARER = "Bearer";
const size_t TOKEN_SIZE = 32;
const size_t AUTHORIZATION_NUMBER_PARTS = 2;
const size_t BEARER_INDEX = 0;
const size_t TOKEN_INDEX = 1;

std::vector<std::string_view> SplitUrl(std::string_view str) {
    std::vector<std::string_view> result;
    
    if (str.empty() || str == "/") {
        return result;
    }
    
    size_t start = 1;
    size_t end = str.find('/', start);
    
    while (end != std::string_view::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find('/', start);
    }
    
    result.push_back(str.substr(start));
    return result;
}

std::string GetTokenString(std::string_view bearer_string) {
    if (bearer_string.empty()) {
        return "";
    }
    
    size_t space_pos = bearer_string.find(' ');
    if (space_pos == std::string_view::npos) {
        return "";
    }
    
    std::string_view bearer_part = bearer_string.substr(0, space_pos);
    std::string_view token_part = bearer_string.substr(space_pos + 1);
    
    if (bearer_part != BEARER || token_part.size() != TOKEN_SIZE) {
        return "";
    }
    
    return std::string(token_part);
}

bool IsEqualUrls(const std::string& server_url, const std::string_view request_url) {
    return request_url == server_url || request_url == server_url + "/";
}

}