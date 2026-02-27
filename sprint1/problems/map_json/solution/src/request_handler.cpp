#include "request_handler.h"
#include <vector>
#include <string>

namespace http_handler {

RequestHandler::RequestHandler(model::Game& game) : game_(game) {
}

bool RequestHandler::IsApiRequest(boost::beast::string_view target) const {
    return target.starts_with("/api/");
}

bool RequestHandler::IsMapRequest(boost::beast::string_view target) const {
    return target.starts_with("/api/v1/maps");
}

std::vector<boost::beast::string_view> RequestHandler::SplitUrl(boost::beast::string_view url) const {
    std::vector<boost::beast::string_view> result;
    
    // Пропускаем начальные слеши
    while (!url.empty() && url.front() == '/') {
        url.remove_prefix(1);
    }
    
    while (!url.empty()) {
        auto pos = url.find('/');
        if (pos == boost::beast::string_view::npos) {
            result.push_back(url);
            break;
        }
        result.push_back(url.substr(0, pos));
        url.remove_prefix(pos + 1);
    }
    
    return result;
}

std::optional<model::Map::Id> RequestHandler::ExtractMapId(boost::beast::string_view target) const {
    auto parts = SplitUrl(target);
    
    // /api/v1/maps/{mapId}
    if (parts.size() >= 4 && parts[0] == "api" && parts[1] == "v1" && parts[2] == "maps") {
        return model::Map::Id(std::string(parts[3]));
    }
    
    return std::nullopt;
}

} // namespace http_handler