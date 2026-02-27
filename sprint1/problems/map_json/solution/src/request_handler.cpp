#include "request_handler.h"

#include <boost/beast/http/status.hpp>

namespace http_handler {

using namespace std::literals;

namespace beast = boost::beast;
namespace http = beast::http;

RequestHandler::RequestHandler(const model::Game& game) : game_(game) {
}

std::vector<std::string_view> RequestHandler::SplitUrl(std::string_view url) const {
    std::vector<std::string_view> result;
    
    if (url.empty() || url == "/"sv) {
        return result;
    }
    
    auto start = url.find_first_not_of('/');
    if (start == std::string_view::npos) {
        return result;
    }
    
    url.remove_prefix(start);
    
    while (!url.empty()) {
        auto end = url.find('/');
        result.push_back(url.substr(0, end));
        
        if (end == std::string_view::npos) {
            break;
        }
        
        url.remove_prefix(end + 1);
    }
    
    return result;
}

bool RequestHandler::IsApiRequest(std::string_view target) const {
    auto parts = SplitUrl(target);
    return !parts.empty() && parts[0] == "api"sv;
}

bool RequestHandler::IsMapRequest(std::string_view target) const {
    auto parts = SplitUrl(target);
    
    if (parts.size() < 3) {
        return false;
    }
    
    return parts[0] == "api"sv && parts[1] == "v1"sv && parts[2] == "maps"sv;
}

std::optional<model::Map::Id> RequestHandler::ExtractMapId(std::string_view target) const {
    auto parts = SplitUrl(target);
    
    if (parts.size() == 4) {
        return model::Map::Id(std::string(parts[3]));
    }
    
    return std::nullopt;
}

}  // namespace http_handler