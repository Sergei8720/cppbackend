#pragma once

#include "model.h"
#include "json_converter.h"
#include <boost/beast.hpp>
#include <optional>
#include <string>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;
using StringRequest = http::request<http::string_body>;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game) : game_(game) {}
    
    template <typename Body, typename Allocator>
    StringResponse HandleRequest(http::request<Body, Allocator>&& req) const {
        if (req.method() != http::verb::get) {
            return MakeMethodNotAllowed(req);
        }
        
        const auto& target = req.target();
        
        if (target == "/api/v1/maps") {
            return MakeMapList(req);
        } else if (target.starts_with("/api/v1/maps/")) {
            return MakeMapById(req, ExtractIdFromPath(target));
        } else {
            return MakePageNotFound(req);
        }
    }
    
private:
    model::Game& game_;
    
    std::string ExtractIdFromPath(std::string_view path) const {
        constexpr std::string_view prefix = "/api/v1/maps/";
        if (path.size() <= prefix.size()) {
            return "";
        }
        return std::string(path.substr(prefix.size()));
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeBadRequest(http::request<Body, Allocator>& req) const {
        StringResponse response(http::status::bad_request, req.version());
        response.set(http::field::content_type, "application/json");
        response.keep_alive(req.keep_alive());
        response.body() = json_converter::CreateBadRequestResponse();
        response.prepare_payload();
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapList(http::request<Body, Allocator>& req) const {
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, "application/json");
        response.keep_alive(req.keep_alive());
        response.body() = json_converter::ConvertMapListToJson(game_);
        response.prepare_payload();
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapById(http::request<Body, Allocator>& req, const std::string& id) const {
        auto map = game_.FindMap(model::Map::Id{id});
        if (!map) {
            return MakeMapNotFound(req);
        }
        
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, "application/json");
        response.keep_alive(req.keep_alive());
        response.body() = json_converter::ConvertMapToJson(*map);
        response.prepare_payload();
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapNotFound(http::request<Body, Allocator>& req) const {
        StringResponse response(http::status::not_found, req.version());
        response.set(http::field::content_type, "application/json");
        response.keep_alive(req.keep_alive());
        response.body() = json_converter::CreateMapNotFoundResponse();
        response.prepare_payload();
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakePageNotFound(http::request<Body, Allocator>& req) const {
        StringResponse response(http::status::not_found, req.version());
        response.set(http::field::content_type, "application/json");
        response.keep_alive(req.keep_alive());
        response.body() = json_converter::CreatePageNotFoundResponse();
        response.prepare_payload();
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeMethodNotAllowed(http::request<Body, Allocator>& req) const {
        StringResponse response(http::status::method_not_allowed, req.version());
        response.set(http::field::content_type, "application/json");
        response.set(http::field::allow, "GET");
        response.keep_alive(req.keep_alive());
        response.body() = json_converter::CreateBadRequestResponse();
        response.prepare_payload();
        return response;
    }
};

}  // namespace http_handler