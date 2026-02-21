#pragma once

#include <boost/beast/http.hpp>
#include <chrono>

#include "logger.h"
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

template <typename RequestHandler>
class LoggingRequestHandler {
 public:
  explicit LoggingRequestHandler(RequestHandler& decorated)
      : decorated_(decorated) {}

  template <typename Body, typename Allocator, typename Send>
  void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                  Send&& send) {
    using namespace std::chrono;
    
    // Логируем получение запроса
    LogRequest(req);
    
    // Засекаем время
    auto start_time = steady_clock::now();
    
    // Вызываем декорируемый обработчик
    decorated_(std::move(req), std::forward<Send>(send));
    
    // Логируем отправку ответа
    auto end_time = steady_clock::now();
    auto response_time = duration_cast<milliseconds>(end_time - start_time).count();
    
    // Примечание: здесь мы не можем сразу залогировать ответ,
    // так как он отправляется асинхронно. Логирование ответа
    // происходит в http_server::SessionBase::Write
  }

 private:
  RequestHandler& decorated_;

  template <typename Body, typename Allocator>
  void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& req) {
    std::string ip;
    try {
      // IP будет добавлен в SessionBase::GetRemoteIp()
    } catch (...) {
    }
    
    // IP будет добавлен позже в SessionBase
    // Здесь мы логируем только то, что доступно
    logware::RequestLogData data;
    data.uri = std::string(req.target());
    data.method = std::string(req.method_string());
    
    // IP будет добавлен в момент логирования в SessionBase
    BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("request received", data);
  }
};

}  // namespace http_handler