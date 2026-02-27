#pragma once
#include <boost/beast/core/string.hpp>
#include <boost/beast/http.hpp>
#include <optional>
#include <vector>
#include <functional>
#include "model.h"
#include "json_converter.h"
#include "http_server.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
public:
    using Handler = std::function<void(http::response<http::string_body>&&)>;

    explicit RequestHandler(model::Game& game);

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        try {
            // Проверяем API запросы
            if (!IsApiRequest(req.target())) {
                SendBadResponse(send, http::status::bad_request, "Bad Request");
                return;
            }

            // Обрабатываем запросы к картам
            if (IsMapRequest(req.target())) {
                auto parts = SplitUrl(req.target());
                
                // /api/v1/maps
                if (parts.size() == 3 && parts[0] == "api" && parts[1] == "v1" && parts[2] == "maps") {
                    if (req.method() == http::verb::get) {
                        auto maps = game_.GetMaps();
                        auto response = json_converter::ConvertMapsToJson(maps);
                        SendResponse(send, response, http::status::ok);
                    } else {
                        SendBadResponse(send, http::status::method_not_allowed, "Method Not Allowed");
                    }
                    return;
                }
                
                // /api/v1/maps/{mapId}
                if (parts.size() == 4 && parts[0] == "api" && parts[1] == "v1" && parts[2] == "maps") {
                    if (req.method() == http::verb::get) {
                        auto map_id = ExtractMapId(req.target());
                        if (map_id.has_value()) {
                            const auto* map = game_.FindMap(*map_id);
                            if (map) {
                                auto response = json_converter::ConvertMapToJson(*map);
                                SendResponse(send, response, http::status::ok);
                            } else {
                                SendBadResponse(send, http::status::not_found, "Map not found");
                            }
                        } else {
                            SendBadResponse(send, http::status::bad_request, "Bad Request");
                        }
                    } else {
                        SendBadResponse(send, http::status::method_not_allowed, "Method Not Allowed");
                    }
                    return;
                }
            }

            // Если ни один маршрут не подошел
            SendBadResponse(send, http::status::bad_request, "Bad Request");
            
        } catch (const std::exception& e) {
            SendBadResponse(send, http::status::internal_server_error, "Internal Server Error");
        }
    }

private:
    model::Game& game_;

    bool IsApiRequest(boost::beast::string_view target) const;
    bool IsMapRequest(boost::beast::string_view target) const;
    std::vector<boost::beast::string_view> SplitUrl(boost::beast::string_view url) const;
    std::optional<model::Map::Id> ExtractMapId(boost::beast::string_view target) const;

    template <typename Send>
    void SendResponse(Send&& send, std::string&& body, http::status status) const {
        http::response<http::string_body> res{status, 11};
        res.set(http::field::content_type, "application/json");
        res.body() = std::move(body);
        res.prepare_payload();
        send(std::move(res));
    }

    template <typename Send>
    void SendBadResponse(Send&& send, http::status status, std::string_view message) const {
        http::response<http::string_body> res{status, 11};
        res.set(http::field::content_type, "application/json");
        
        // Формируем JSON с ошибкой
        std::string body = "{\"code\":\"" + std::to_string(static_cast<int>(status)) + 
                          "\",\"message\":\"" + std::string(message) + "\"}";
        res.body() = std::move(body);
        res.prepare_payload();
        send(std::move(res));
    }
};

} // namespace http_handler