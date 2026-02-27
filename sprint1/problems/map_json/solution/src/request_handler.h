#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "http_server.h"
#include "model.h"
#include "json_converter.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
using StringResponse = http::response<http::string_body>;

class RequestHandler {
public:
    explicit RequestHandler(const model::Game& game);
    
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;
    RequestHandler(RequestHandler&&) = delete;
    RequestHandler& operator=(RequestHandler&&) = delete;
    
    ~RequestHandler() = default;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
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

private:
    const model::Game& game_;

    std::vector<std::string_view> SplitUrl(std::string_view url) const;
    bool IsApiRequest(std::string_view target) const;
    bool IsMapRequest(std::string_view target) const;
    std::optional<model::Map::Id> ExtractMapId(std::string_view target) const;
    
    template <typename Body, typename Allocator>
    StringResponse MakeBadRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        StringResponse response(http::status::bad_request, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = jsonConverter::CreateBadRequestResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapList(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = jsonConverter::ConvertMapListToJson(game_);
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapById(const http::request<Body, http::basic_fields<Allocator>>& req,
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
    StringResponse MakeMapNotFound(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        StringResponse response(http::status::not_found, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = jsonConverter::CreateMapNotFoundResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakePageNotFound(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        StringResponse response(http::status::not_found, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = jsonConverter::CreatePageNotFoundResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }
    
    template <typename Body, typename Allocator>
    StringResponse MakeMethodNotAllowed(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        StringResponse response(http::status::method_not_allowed, req.version());
        response.set(http::field::content_type, "application/json");
        response.set(http::field::allow, "GET");
        response.body() = jsonConverter::CreateBadRequestResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        return response;
    }
};

}  // namespace http_handler