#pragma once
#include "model.h"
#include "json_converter.h"
#include "request_handlers_utility.h"

#include <vector>
#include <boost/beast/http.hpp>

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;

const size_t kTwoSegmentUrl = 2;
const size_t kThreeSegmentUrl = 3;
const size_t kFourSegmentUrl = 4;

template <typename Request>
bool GetMapListActivator(const Request& req, const model::Game& game) {
  return req.target() == "/api/v1/maps" || req.target() == "/api/v1/maps/";
}

template <typename Request, typename Send>
void GetMapListHandler(const Request& req, const model::Game& game, Send&& send) {
  StringResponse response(http::status::ok, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::ConvertMapListToJson(game);
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

template <typename Request>
bool GetMapByIdActivator(const Request& req, const model::Game& game) {
  auto url = SplitUrl(req.target());
  
  return url.size() == kFourSegmentUrl &&
         url[0] == "api" &&
         url[1] == "v1" &&
         url[2] == "maps" &&
         game.FindMap(model::Map::Id(std::string(url[3]))) != nullptr;
}

template <typename Request, typename Send>
void GetMapByIdHandler(const Request& req, const model::Game& game, Send&& send) {
  auto url_parts = SplitUrl(req.target());
  auto map_id = model::Map::Id(std::string(url_parts[3]));
  
  StringResponse response(http::status::ok, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::ConvertMapToJson(*game.FindMap(map_id));
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

template <typename Request>
bool BadRequestActivator(const Request& req, const model::Game& game) {
  auto url = SplitUrl(req.target());
  
  return !url.empty() &&
         url[0] == "api" &&
         (url.size() > kFourSegmentUrl ||
          url.size() < kThreeSegmentUrl ||
          (url.size() >= kTwoSegmentUrl && url[1] != "v1") ||
          (url.size() >= kThreeSegmentUrl && url[2] != "maps"));
}

template <typename Request, typename Send>
void BadRequestHandler(const Request& req, const model::Game& game, Send&& send) {
  StringResponse response(http::status::bad_request, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreateBadRequestResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

template <typename Request>
bool MapNotFoundActivator(const Request& req, const model::Game& game) {
  auto url = SplitUrl(req.target());
  
  return url.size() == kFourSegmentUrl &&
         url[0] == "api" &&
         url[1] == "v1" &&
         url[2] == "maps" &&
         game.FindMap(model::Map::Id(std::string(url[3]))) == nullptr;
}

template <typename Request, typename Send>
void MapNotFoundHandler(const Request& req, const model::Game& game, Send&& send) {
  StringResponse response(http::status::not_found, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreateMapNotFoundResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

template <typename Request, typename Send>
void PageNotFoundHandler(const Request& req, const model::Game& game, Send&& send) {
  StringResponse response(http::status::not_found, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreatePageNotFoundResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

}  // namespace rh_storage