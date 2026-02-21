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
    auto start_time = std::chrono::steady_clock::now();
    
    decorated_(std::move(req), 
               [this, send = std::forward<Send>(send), start_time]
               (auto&& response) mutable {
                 auto end_time = std::chrono::steady_clock::now();
                 auto response_time = 
                     std::chrono::duration_cast<std::chrono::milliseconds>(
                         end_time - start_time).count();
                 
                 // Здесь нужно будет добавить IP из сессии
                 // IP будет добавлен в http_server
                 
                 send(std::forward<decltype(response)>(response));
               });
  }

 private:
  RequestHandler& decorated_;
};

}  // namespace http_handler