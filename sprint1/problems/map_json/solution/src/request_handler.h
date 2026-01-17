#ifndef REQUEST_HANDLER_H_
#define REQUEST_HANDLER_H_

#include <string>
#include <string_view>
#include <vector>

#include <boost/beast/http.hpp>

#include "model.h"
#include "json_converter.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
 public:
    explicit RequestHandler(const model::Game& game) : game_(game) {}
    
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto response = HandleRequest(req);
        send(std::move(response));
    }

 private:
    const model::Game& game_;

    static std::vector<std::string> SplitTarget(beast::string_view target) {
        std::vector<std::string> parts;
        std::string target_str(target.data(), target.size());
        
        if (target_str.empty() || target_str == "/") {
            return parts;
        }
        
        size_t start = target_str.starts_with('/') ? 1 : 0;
        size_t end = target_str.find('/', start);
        
        while (end != std::string::npos) {
            parts.push_back(target_str.substr(start, end - start));
            start = end + 1;
            end = target_str.find('/', start);
        }
        
        if (start < target_str.length()) {
            parts.push_back(target_str.substr(start));
        }
        
        return parts;
    }

    static bool IsValidApiRequest(const std::vector<std::string>& parts) {
        if (parts.size() < 3) {
            return false;
        }
        
        return parts[0] == "api" && parts[1] == "v1" && parts[2] == "maps";
    }

    static bool IsValidMapIdRequest(const std::vector<std::string>& parts) {
        return parts.size() == 4;
    }

    static bool IsValidMapListRequest(const std::vector<std::string>& parts) {
        return parts.size() == 3;
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) const {
        
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeMethodNotAllowedResponse(req);
        }
        
        auto parts = SplitTarget(req.target());
        
        if (!IsValidApiRequest(parts)) {
            return MakePageNotFoundResponse(req);
        }
        
        if (IsValidMapListRequest(parts)) {
            return MakeMapListResponse(req);
        }
        
        if (IsValidMapIdRequest(parts)) {
            return MakeMapByIdResponse(req, parts[3]);
        }
        
        return MakeBadRequestResponse(req);
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeResponse(
        http::status status,
        const http::request<Body, http::basic_fields<Allocator>>& req,
        std::string body) const {
        
        http::response<http::string_body> response{status, req.version()};
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = std::move(body);
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMapListResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req) const {
        
        return MakeResponse(http::status::ok, req, json_converter::ConvertMapListToJson(game_));
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMapByIdResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        const std::string& map_id) const {
        
        auto map = game_.FindMap(model::Map::Id{map_id});
        
        if (!map) {
            return MakeResponse(http::status::not_found, req,
                               json_converter::CreateMapNotFoundResponse());
        }
        
        return MakeResponse(http::status::ok, req, json_converter::ConvertMapToJson(*map));
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeBadRequestResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req) const {
        
        return MakeResponse(http::status::bad_request, req,
                           json_converter::CreateBadRequestResponse());
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakePageNotFoundResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req) const {
        
        return MakeResponse(http::status::not_found, req,
                           json_converter::CreatePageNotFoundResponse());
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeMethodNotAllowedResponse(
        const http::request<Body, http::basic_fields<Allocator>>& req) const {
        
        http::response<http::string_body> response{http::status::method_not_allowed, req.version()};
        response.set(http::field::allow, "GET, HEAD");
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = json_converter::CreateBadRequestResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }
};

}  // namespace http_handler

#endif  // REQUEST_HANDLER_H_