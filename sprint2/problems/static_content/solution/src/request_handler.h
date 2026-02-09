#ifndef REQUEST_HANDLER_H_
#define REQUEST_HANDLER_H_

#include "http_server.h"
#include "model.h"
#include "api_v1_response_storage.h"
#include "static_file_response_storage.h"

#include <filesystem>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

StringResponse MakeMethodNotAllowedResponse(
    const StringRequest& req, std::string_view allowed_methods);

class RequestHandler {
 public:
  explicit RequestHandler(model::Game& game, fs::path static_content_root_path)
      : game_(game),
        static_content_root_path_(std::move(static_content_root_path)) {
  }

  RequestHandler(const RequestHandler&) = delete;
  RequestHandler& operator=(const RequestHandler&) = delete;

  template <typename Body, typename Allocator, typename Send>
  void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                  Send&& send) {
    if (IsApiRequest(req.target())) {
      HandleApiRequest(std::move(req), std::forward<Send>(send));
    } else {
      HandleStaticRequest(std::move(req), std::forward<Send>(send));
    }
  }

 private:
  model::Game& game_;
  fs::path static_content_root_path_;

  bool IsApiRequest(std::string_view target) const {
    return target.starts_with("/api/");
  }

  template <typename Body, typename Allocator, typename Send>
  void HandleApiRequest(
      http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (!IsValidHttpMethodForApi(req.method())) {
      send(http_handler::MakeMethodNotAllowedResponse(
          StringRequest(req), "GET, HEAD"));
      return;
    }

    if (response_storage::UseBadRequestActivator(req, game_)) {
      send(response_storage::MakeBadRequestResponse(req, game_));
    } else if (response_storage::UseGetMapListActivator(req, game_)) {
      send(response_storage::MakeGetMapListResponse(req, game_));
    } else if (response_storage::UseMapNotFoundActivator(req, game_)) {
      send(response_storage::MakeMapNotFoundResponse(req, game_));
    } else if (response_storage::UseGetMapByIdActivator(req, game_)) {
      send(response_storage::MakeGetMapByIdResponse(req, game_));
    } else {
      send(response_storage::MakePageNotFoundResponse(req, game_));
    }
  }

  template <typename Body, typename Allocator, typename Send>
  void HandleStaticRequest(
      http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (!IsValidHttpMethodForStatic(req.method())) {
      send(http_handler::MakeMethodNotAllowedResponse(
          StringRequest(req), "GET, HEAD"));
      return;
    }

    if (response_storage::UseStaticContentFileNotFoundActivator(
        req, static_content_root_path_)) {
      send(response_storage::MakeStaticContentFileNotFoundResponse(
          req, static_content_root_path_));
    } else if (response_storage::UseLeaveStaticContentRootDirActivator(
        req, static_content_root_path_)) {
      send(response_storage::MakeLeaveStaticContentRootDirResponse(
          req, static_content_root_path_));
    } else {
      send(response_storage::MakeGetStaticContentFileResponse(
          req, static_content_root_path_));
    }
  }

  bool IsValidHttpMethodForApi(http::verb method) const {
    return method == http::verb::get || method == http::verb::head;
  }

  bool IsValidHttpMethodForStatic(http::verb method) const {
    return method == http::verb::get || method == http::verb::head;
  }
};

}  // namespace http_handler

#endif  // REQUEST_HANDLER_H_