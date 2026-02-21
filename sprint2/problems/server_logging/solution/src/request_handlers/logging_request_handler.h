#pragma once

#include <boost/beast/http.hpp>
#include <chrono>

#include "logger.h"
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

template <typename RequestHandler>
class LoggingRequestHandler {
 public:
  explicit LoggingRequestHandler(RequestHandler& decorated)
      : decorated_(decorated) {}

  template <typename Body, typename Allocator, typename Send>
  void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                  Send&& send) {
    decorated_(std::move(req), std::forward<Send>(send));
  }

 private:
  RequestHandler& decorated_;
};

}  // namespace http_handler