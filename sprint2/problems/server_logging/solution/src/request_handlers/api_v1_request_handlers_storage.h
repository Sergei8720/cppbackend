#pragma once

#include <boost/beast/http.hpp>
#include <vector>

#include "json_converter.h"
#include "model.h"
#include "request_handlers_utility.h"

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;

constexpr size_t kSizeOfTwoSegmentUrl = 2;
constexpr size_t kSizeOfThreeSegmentUrl = 3;
constexpr size_t kSizeOfFourSegmentUrl = 4;

template <typename Request>
bool GetMapListActivator(const Request& req, const model::Game& game) {
  return (req.target() == "/api/v1/maps") ||
         (req.target() == "/api/v1/maps/");
}

template <typename Request, typename Send>
void GetMapListHandler(const Request& req, const model::Game& game,
                       Send&& send) {
  StringResponse response(http::status::ok, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::ConvertMapListToJson(game);
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(response);
}

template <typename Request>
bool GetMapByIdActivator(const Request& req, const model::Game& game) {
  auto url_parts = SplitUrl(req.target());
  return (url_parts.size() == kSizeOfFourSegmentUrl) &&
         (url_parts[0] == "api") && (url_parts[1] == "v1") &&
         (url_parts[2] == "maps") &&
         (game.FindMap(model::Map::Id(std::string(url_parts[3]))) != nullptr);
}

template <typename Request, typename Send>
void GetMapByIdHandler(const Request& req, const model::Game& game,
                       Send&& send) {
  auto url_parts = SplitUrl(req.target());
  auto id = url_parts[3];

  StringResponse response(http::status::ok, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::ConvertMapToJson(
      *game.FindMap(model::Map::Id(std::string(id))));
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(response);
}

template <typename Request>
bool BadRequestActivator(const Request& req, const model::Game& game) {
  auto url_parts = SplitUrl(req.target());
  return !url_parts.empty() && (url_parts[0] == "api") &&
         ((url_parts.size() > kSizeOfFourSegmentUrl) ||
          (url_parts.size() < kSizeOfThreeSegmentUrl) ||
          ((url_parts.size() >= kSizeOfTwoSegmentUrl) &&
           (url_parts[1] != "v1")) ||
          ((url_parts.size() >= kSizeOfThreeSegmentUrl) &&
           (url_parts[2] != "maps")));
}

template <typename Request, typename Send>
void BadRequestHandler(const Request& req, const model::Game& game,
                       Send&& send) {
  StringResponse response(http::status::bad_request, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreateBadRequestResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(response);
}

template <typename Request>
bool MapNotFoundActivator(const Request& req, const model::Game& game) {
  auto url_parts = SplitUrl(req.target());
  return (url_parts.size() == kSizeOfFourSegmentUrl) &&
         (url_parts[0] == "api") && (url_parts[1] == "v1") &&
         (url_parts[2] == "maps") &&
         (game.FindMap(model::Map::Id(std::string(url_parts[3]))) == nullptr);
}

template <typename Request, typename Send>
void MapNotFoundHandler(const Request& req, const model::Game& game,
                        Send&& send) {
  StringResponse response(http::status::not_found, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreateMapNotFoundResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(response);
}

template <typename Request, typename Send>
void PageNotFoundHandler(const Request& req, const model::Game& game,
                         Send&& send) {
  StringResponse response(http::status::not_found, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreatePageNotFoundResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(response);
}

}  // namespace rh_storage