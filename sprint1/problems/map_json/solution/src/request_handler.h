#pragma once

#include "http_server.h"
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

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
        HandleRequest(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
    }

private:
    const model::Game& game_;

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);
    
    std::vector<std::string_view> SplitUrl(std::string_view url) const;
    bool IsApiRequest(std::string_view target) const;
    bool IsMapRequest(std::string_view target) const;
    std::optional<model::Map::Id> ExtractMapId(std::string_view target) const;
    
    template <typename Body, typename Allocator>
    StringResponse MakeBadRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const;
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapList(const http::request<Body, http::basic_fields<Allocator>>& req) const;
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapById(const http::request<Body, http::basic_fields<Allocator>>& req,
                              const model::Map::Id& map_id) const;
    
    template <typename Body, typename Allocator>
    StringResponse MakeMapNotFound(const http::request<Body, http::basic_fields<Allocator>>& req) const;
    
    template <typename Body, typename Allocator>
    StringResponse MakePageNotFound(const http::request<Body, http::basic_fields<Allocator>>& req) const;
    
    template <typename Body, typename Allocator>
    StringResponse MakeMethodNotAllowed(const http::request<Body, http::basic_fields<Allocator>>& req) const;
};

}  // namespace http_handler