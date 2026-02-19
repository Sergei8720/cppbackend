#pragma once
#include <unordered_map>
#include <boost/beast/http.hpp>

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;

template<typename Activator, typename Handler>
class RequestHandlerNode {
 public:
  RequestHandlerNode(Activator activator, std::unordered_map<http::verb, Handler> handlers)
    : activator_(std::move(activator))
    , handlers_(std::move(handlers)) {}

  RequestHandlerNode(const RequestHandlerNode&) = default;
  RequestHandlerNode(RequestHandlerNode&&) = default;
  RequestHandlerNode& operator=(const RequestHandlerNode&) = default;
  RequestHandlerNode& operator=(RequestHandlerNode&&) = default;
  
  ~RequestHandlerNode() = default;

  template<typename Request>
  const Handler& GetHandler(const Request& req, const Handler& fault_handler) const {
    auto it = handlers_.find(req.method());
    if (it != handlers_.end()) {
      return it->second;
    }
    return fault_handler;
  }

  const Activator& GetActivator() const {
    return activator_;
  }

 private:
  Activator activator_;
  std::unordered_map<http::verb, Handler> handlers_;
};

}  // namespace rh_storage