#include "request_handler.h"

namespace http_handler {

StringResponse MakeMethodNotAllowedResponse(
    const StringRequest& req, std::string_view allowed_methods) {
  StringResponse response(http::status::method_not_allowed, req.version());
  response.set(http::field::content_type, "text/plain");
  response.set(http::field::allow, allowed_methods);
  response.body() = "Method not allowed";
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

}  // namespace http_handler