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

const size_t SizeOfTwoSegmentUrl = 2;
const size_t SizeOfThreeSegmentUrl = 3;
const size_t SizeOfFourSegmentUrl = 4;

template <typename Request>
bool GetMapListActivator(const Request& req, const model::Game& game) {
    std::string target(req.target());
    return target == "/api/v1/maps" || target == "/api/v1/maps/";
}

template <typename Request, typename Send>
void GetMapListHandler(const Request& req, const model::Game& game, Send&& send) {
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = json_converter::ConvertMapListToJson(game);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request>
bool GetMapByIdActivator(const Request& req, const model::Game& game) {
    auto url = SplitUrl(req.target());
    if (url.size() != SizeOfFourSegmentUrl) {
        return false;
    }
    if (url[0] != "api" || url[1] != "v1" || url[2] != "maps") {
        return false;
    }
    return game.FindMap(model::Map::Id(std::string(url[3]))) != nullptr;
}

template <typename Request, typename Send>
void GetMapByIdHandler(const Request& req, const model::Game& game, Send&& send) {
    http::response<http::string_body> response(http::status::ok, req.version());
    auto url_parts = SplitUrl(req.target());
    std::string id_str(url_parts[3]);
    response.set(http::field::content_type, "application/json");
    response.body() = json_converter::ConvertMapToJson(*game.FindMap(model::Map::Id(id_str)));
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request>
bool BadRequestActivator(const Request& req, const model::Game& game) {
    auto url = SplitUrl(req.target());
    if (url.empty()) {
        return false;
    }
    if (url[0] != "api") {
        return false;
    }
    if (url.size() > SizeOfFourSegmentUrl || url.size() < SizeOfThreeSegmentUrl) {
        return true;
    }
    if (url.size() >= SizeOfTwoSegmentUrl && url[1] != "v1") {
        return true;
    }
    if (url.size() >= SizeOfThreeSegmentUrl && url[2] != "maps") {
        return true;
    }
    return false;
}

template <typename Request, typename Send>
void BadRequestHandler(const Request& req, const model::Game& game, Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = json_converter::CreateBadRequestResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request>
bool MapNotFoundActivator(const Request& req, const model::Game& game) {
    auto url = SplitUrl(req.target());
    if (url.size() != SizeOfFourSegmentUrl) {
        return false;
    }
    if (url[0] != "api" || url[1] != "v1" || url[2] != "maps") {
        return false;
    }
    return game.FindMap(model::Map::Id(std::string(url[3]))) == nullptr;
}

template <typename Request, typename Send>
void MapNotFoundHandler(const Request& req, const model::Game& game, Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = json_converter::CreateMapNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request, typename Send>
void PageNotFoundHandler(const Request& req, const model::Game& game, Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, "application/json");
    response.body() = json_converter::CreatePageNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

}