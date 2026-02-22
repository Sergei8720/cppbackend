#ifndef LOGGING_REQUEST_HANDLER_H_
#define LOGGING_REQUEST_HANDLER_H_

#include "request_handler.h"
#include "logger.h"

#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>

namespace beast = boost::beast;
namespace http = beast::http;

template <typename RequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(RequestHandler& decorated)
        : decorated_(decorated) {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::chrono;
        
        // Для получения IP-адреса нам нужен доступ к сокету.
        // В реальном проекте IP нужно передавать через контекст.
        // Пока используем заглушку.
        std::string ip = "unknown";

        // Логируем получение запроса
        boost::json::value request_data{
            {"ip", ip},
            {"URI", std::string(req.target())},
            {"method", std::string(http::to_string(req.method()))}
        };
        
        BOOST_LOG_TRIVIAL(info) << logging::add_value(logging::additional_data, request_data)
                                << "request received";

        // Засекаем время начала обработки
        auto start_time = steady_clock::now();

        // Передаем запрос дальше
        decorated_(std::move(req), [this, start_time, send = std::forward<Send>(send)]
                   (auto&& response) {
            // Вычисляем время ответа
            auto elapsed_ms = duration_cast<milliseconds>(steady_clock::now() - start_time).count();

            // Логируем отправку ответа
            std::string content_type_str = "null";
            auto it = response.find(http::field::content_type);
            if (it != response.end()) {
                content_type_str = std::string(it->value());
            }

            boost::json::value response_data{
                {"response_time", static_cast<long long>(elapsed_ms)},
                {"code", response.result_int()},
                {"content_type", content_type_str}
            };

            BOOST_LOG_TRIVIAL(info) << logging::add_value(logging::additional_data, response_data)
                                    << "response sent";

            // Отправляем ответ
            send(std::forward<decltype(response)>(response));
        });
    }

private:
    RequestHandler& decorated_;
};

#endif // LOGGING_REQUEST_HANDLER_H_