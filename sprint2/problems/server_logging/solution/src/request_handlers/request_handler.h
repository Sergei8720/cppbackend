#pragma once

#include <filesystem>

#include "api_v1_request_handlers_executor.h"
#include "http_server.h"
#include "model.h"
#include "static_file_request_handlers_executor.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;
namespace fs = std::filesystem;

class RequestHandler {
 public:
  explicit RequestHandler(model::Game& game,
                          fs::path static_content_root_path)
      : game_(game), static_content_root_path_(static_content_root_path) {}

  RequestHandler(const RequestHandler&) = delete;
  RequestHandler& operator=(const RequestHandler&) = delete;

  template <typename Body, typename Allocator, typename Send>
  void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                  Send&& send) {
    using RequestType = http::request<Body, http::basic_fields<Allocator>>;

    if (rh_storage::ApiV1RequestHandlerExecutor<RequestType, Send>::
            GetInstance()
                .Execute(req, game_, std::move(send))) {
      return;
    }

    if (rh_storage::StaticFileRequestHandlerExecutor<RequestType, Send>::
            GetInstance()
                .Execute(req, static_content_root_path_, std::move(send))) {
      return;
    }

    rh_storage::PageNotFoundHandler(req, game_, send);
  }

 private:
  model::Game& game_;
  fs::path static_content_root_path_;
};

}  // namespace http_handler