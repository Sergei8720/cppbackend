#ifndef API_V1_RESPONSE_STORAGE_H_
#define API_V1_RESPONSE_STORAGE_H_

#include "model.h"
#include "json_converter.h"

#include <boost/beast/http.hpp>
#include <vector>

namespace response_storage {

namespace beast = boost::beast;
namespace http = beast::http;

constexpr size_t kSizeOfTwoSegmentUrl = 2;
constexpr size_t kSizeOfThreeSegmentUrl = 3;
constexpr size_t kSizeOfFourSegmentUrl = 4;

std::vector<std::string_view> SplitUrl(std::string_view str);

template <typename Body, typename Allocator>
bool UseGetMapListActivator(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  return req.target() == "/api/v1/maps" || req.target() == "/api/v1/maps/";
}

template <typename Body, typename Allocator>
http::response<http::string_body> MakeGetMapListResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  http::response<http::string_body> response(http::status::ok, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::ConvertMapListToJson(game);
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

template <typename Body, typename Allocator>
bool UseGetMapByIdActivator(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  auto url = SplitUrl(req.target());
  if (url.size() != kSizeOfFourSegmentUrl) {
    return false;
  }
  
  return url[0] == "api" &&
         url[1] == "v1" &&
         url[2] == "maps" &&
         game.FindMap(model::Map::Id(std::string(url[3]))) != nullptr;
}

template <typename Body, typename Allocator>
http::response<http::string_body> MakeGetMapByIdResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  http::response<http::string_body> response(http::status::ok, req.version());
  auto id = SplitUrl(req.target())[3];
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::ConvertMapToJson(
      *game.FindMap(model::Map::Id(std::string(id))));
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

template <typename Body, typename Allocator>
bool UseBadRequestActivator(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  auto url = SplitUrl(req.target());
  if (url.empty()) {
    return false;
  }
  
  if (url[0] != "api") {
    return false;
  }
  
  if (url.size() > kSizeOfFourSegmentUrl || url.size() < kSizeOfThreeSegmentUrl) {
    return true;
  }
  
  if (url.size() >= kSizeOfTwoSegmentUrl && url[1] != "v1") {
    return true;
  }
  
  if (url.size() >= kSizeOfThreeSegmentUrl && url[2] != "maps") {
    return true;
  }
  
  return false;
}

template <typename Body, typename Allocator>
http::response<http::string_body> MakeBadRequestResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  http::response<http::string_body> response(
      http::status::bad_request, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreateBadRequestResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

template <typename Body, typename Allocator>
bool UseMapNotFoundActivator(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  auto url = SplitUrl(req.target());
  if (url.size() != kSizeOfFourSegmentUrl) {
    return false;
  }
  
  return url[0] == "api" &&
         url[1] == "v1" &&
         url[2] == "maps" &&
         game.FindMap(model::Map::Id(std::string(url[3]))) == nullptr;
}

template <typename Body, typename Allocator>
http::response<http::string_body> MakeMapNotFoundResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  http::response<http::string_body> response(
      http::status::not_found, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreateMapNotFoundResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

template <typename Body, typename Allocator>
http::response<http::string_body> MakePageNotFoundResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const model::Game& game) {
  http::response<http::string_body> response(
      http::status::not_found, req.version());
  response.set(http::field::content_type, "application/json");
  response.body() = json_converter::CreatePageNotFoundResponse();
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

}  // namespace response_storage

#endif  // API_V1_RESPONSE_STORAGE_H_