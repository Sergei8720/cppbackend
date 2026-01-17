#include "request_handler.h"
#include "json_converter.h"

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

template <typename Body, typename Allocator>
StringResponse RequestHandler::MakeBadRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = jsonConverter::CreateBadRequestResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    return response;
}

template <typename Body, typename Allocator>
StringResponse RequestHandler::MakeMapList(const http::request<Body, http::basic_fields<Allocator>>& req) const {
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = jsonConverter::ConvertMapListToJson(game_);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    return response;
}

template <typename Body, typename Allocator>
StringResponse RequestHandler::MakeMapById(const http::request<Body, http::basic_fields<Allocator>>& req,
                                          const model::Map::Id& map_id) const {
    const auto* map = game_.FindMap(map_id);
    
    if (!map) {
        return MakeMapNotFound(req);
    }
    
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = jsonConverter::ConvertMapToJson(*map);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    return response;
}

template <typename Body, typename Allocator>
StringResponse RequestHandler::MakeMapNotFound(const http::request<Body, http::basic_fields<Allocator>>& req) const {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = jsonConverter::CreateMapNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    return response;
}

template <typename Body, typename Allocator>
StringResponse RequestHandler::MakePageNotFound(const http::request<Body, http::basic_fields<Allocator>>& req) const {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = jsonConverter::CreatePageNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    return response;
}

template <typename Body, typename Allocator>
StringResponse RequestHandler::MakeMethodNotAllowed(const http::request<Body, http::basic_fields<Allocator>>& req) const {
    StringResponse response(http::status::method_not_allowed, req.version());
    response.set(http::field::content_type, "application/json");
    response.set(http::field::allow, "GET");
    response.body() = jsonConverter::CreateBadRequestResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    return response;
}

template <typename Body, typename Allocator, typename Send>
void RequestHandler::operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (!IsApiRequest(req.target())) {
        send(MakePageNotFound(req));
        return;
    }
    
    if (req.method() != http::verb::get) {
        send(MakeMethodNotAllowed(req));
        return;
    }
    
    if (IsMapRequest(req.target())) {
        auto parts = SplitUrl(req.target());
        
        if (parts.size() == 3) {
            send(MakeMapList(req));
        } else if (parts.size() == 4) {
            auto map_id = ExtractMapId(req.target());
            
            if (map_id) {
                send(MakeMapById(req, *map_id));
            } else {
                send(MakeBadRequest(req));
            }
        } else {
            send(MakeBadRequest(req));
        }
    } else {
        send(MakeBadRequest(req));
    }
}

}  // namespace http_handler