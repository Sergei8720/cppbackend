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

        // Логируем получение запроса
        LogRequest(req);

        // Засекаем время начала обработки
        auto start_time = steady_clock::now();

        // Передаем запрос дальше
        decorated_(std::move(req), [this, start_time, send = std::forward<Send>(send)]
                   (auto&& response) {
            // Вычисляем время ответа
            auto elapsed_ms = duration_cast<milliseconds>(steady_clock::now() - start_time).count();

            // Логируем отправку ответа
            LogResponse(response, elapsed_ms);

            // Отправляем ответ
            send(std::forward<decltype(response)>(response));
        });
    }

private:
    RequestHandler& decorated_;

    template <typename Body, typename Allocator>
    void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& req) {
        // Получаем IP-адрес клиента (это заглушка, так как в этом месте у нас нет сокета)
        // В реальном проекте IP можно получить из сессии или передать его через контекст.
        // Пока оставим заглушку.
        std::string ip = "unknown";

        boost::json::value data{
            {"ip", ip},
            {"URI", std::string(req.target())},
            {"method", std::string(http::to_string(req.method()))}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(logging::additional_data, data)
                                << "request received";
    }

    template <typename Body, typename Fields>
    void LogResponse(const http::response<Body, Fields>& resp, long long response_time_ms) {
        std::string content_type_str = "null";
        auto it = resp.find(http::field::content_type);
        if (it != resp.end()) {
            content_type_str = std::string(it->value());
        }

        boost::json::value data{
            {"response_time", response_time_ms},
            {"code", resp.result_int()},
            {"content_type", content_type_str}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(logging::additional_data, data)
                                << "response sent";
    }
};

#endif // LOGGING_REQUEST_HANDLER_H_