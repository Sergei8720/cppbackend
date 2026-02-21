#pragma once

#include <boost/beast/http.hpp>
#include <unordered_map>

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;

template <typename Activator, typename Handler>
class RequestHandlerNode {
 public:
  RequestHandlerNode(Activator activator,
                     std::unordered_map<http::verb, Handler> handlers)
      : activator_(std::move(activator)), handlers_(std::move(handlers)) {}

  RequestHandlerNode(const RequestHandlerNode&) = default;
  RequestHandlerNode(RequestHandlerNode&&) = default;
  RequestHandlerNode& operator=(const RequestHandlerNode&) = default;
  RequestHandlerNode& operator=(RequestHandlerNode&&) = default;

  virtual ~RequestHandlerNode() = default;

  template <typename Request>
  Handler& GetHandler(const Request& req, Handler& fault_handler) {
    http::verb method = req.method();
    auto iterator = handlers_.find(method);
    if (iterator != handlers_.end()) {
      return iterator->second;
    }
    return fault_handler;
  }

  Activator& GetActivator() { return activator_; }

 private:
  Activator activator_;
  std::unordered_map<http::verb, Handler> handlers_;
};

}  // namespace rh_storage