#pragma once
#include "application.h"
#include "player_tokens.h"
#include "json_converter.h"
#include "request_handlers_utils.h"
#include "api_url_storage.h"

#include <vector>
#include <optional>
#include <boost/beast/http.hpp>
#include <chrono>
#include <unordered_set>

namespace rh_storage{

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using StringResponse = http::response<http::string_body>;

const size_t SIZE_OF_TWO_SEGMENT_URL = 2;
const size_t SIZE_OF_THREE_SEGMENT_URL = 3;
const size_t SIZE_OF_FOUR_SEGMENT_URL = 4;
const size_t SIZE_OF_FIVE_SEGMENT_URL = 5;

const std::unordered_set<std::string_view> GAME_API_URLS_WITH_AUTHORIZATION = {
    api_urls::GET_PLAYERS_LIST_API,
    api_urls::GET_PLAYERS_LIST_API + "/",
    api_urls::GET_GAME_STATE_API,
    api_urls::GET_GAME_STATE_API + "/",
    api_urls::MAKE_ACTION_API,
    api_urls::MAKE_ACTION_API + "/"
};

const std::unordered_set<std::string_view> GAME_API_URLS_WITH_JSON_REQ = {
    api_urls::JOIN_TO_GAME_API,
    api_urls::JOIN_TO_GAME_API + "/",
    api_urls::MAKE_ACTION_API,
    api_urls::MAKE_ACTION_API + "/"
};

const std::string CONTENT_TYPE_APPLICATION_JSON = "application/json";
const std::string NO_CACHE_CONTROL = "no-cache";

template <typename Request>
bool BadRequestActivator(const Request& req) {
    auto url = SplitUrl(req.target());
    if (url.empty() || url[0] != "api") {
        return false;
    }
    
    if (url.size() > SIZE_OF_FIVE_SEGMENT_URL || url.size() < SIZE_OF_THREE_SEGMENT_URL) {
        return true;
    }
    
    if (url.size() >= SIZE_OF_TWO_SEGMENT_URL && url[1] != "v1") {
        return true;
    }
    
    if (url.size() >= SIZE_OF_THREE_SEGMENT_URL) {
        if (url[2] == "maps") {
            return url.size() > SIZE_OF_FOUR_SEGMENT_URL;
        }
        if (url[2] == "game") {
            if (url.size() == SIZE_OF_THREE_SEGMENT_URL) {
                return true;
            }
            if (url[3] == "join" && url.size() > SIZE_OF_FOUR_SEGMENT_URL) {
                return true;
            }
            if (url[3] == "players" && url.size() > SIZE_OF_FOUR_SEGMENT_URL) {
                return true;
            }
            if (url[3] == "state" && url.size() > SIZE_OF_FOUR_SEGMENT_URL) {
                return true;
            }
            if (url[3] == "player") {
                if (url.size() == SIZE_OF_FOUR_SEGMENT_URL) {
                    return true;
                }
                if (url[4] != "action" || url.size() > SIZE_OF_FIVE_SEGMENT_URL) {
                    return true;
                }
            }
            if (url[3] == "tick" && url.size() > SIZE_OF_FOUR_SEGMENT_URL) {
                return true;
            }
        }
    }
    return false;
}

template <typename Request, typename Send>
std::optional<size_t> BadRequestHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.body() = json_converter::CreateBadRequestResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool GetMapListActivator(const Request& req) {
    return IsEqualUrls(api_urls::GET_MAPS_LIST_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> GetMapListHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::ConvertMapListToJson(application.ListMap());
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool GetMapByIdActivator(const Request& req) {
    auto url = SplitUrl(req.target());
    return url.size() == SIZE_OF_FOUR_SEGMENT_URL &&
            url[0] == "api" &&
            url[1] == "v1" &&
            url[2] == "maps";
}

template <typename Request, typename Send>
std::optional<size_t> GetMapByIdHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    auto url = SplitUrl(req.target());
    auto id = url[3];
    auto map = application.FindMap(model::Map::Id(std::string(id)));
    if(map == nullptr) {
        return 0;
    }
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::ConvertMapToJson(*map);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> MapNotFoundHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.body() = json_converter::CreateMapNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool JoinToGameInvalidJsonReqActivator(const Request& req) {
    return IsEqualUrls(api_urls::JOIN_TO_GAME_API, req.target()) &&
        !json_converter::ParseJoinToGameRequest(req.body()).has_value();
}

template <typename Request, typename Send>
std::optional<size_t> JoinToGameInvalidJsonReqHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateJoinToGameInvalidArgumentResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool JoinToGameEmptyPlayerNameActivator(const Request& req) {
    if (!IsEqualUrls(api_urls::JOIN_TO_GAME_API, req.target())) {
        return false;
    }
    auto res = json_converter::ParseJoinToGameRequest(req.body());
    if (!res) {
        return false;
    }
    auto [player_name, map_id] = res.value();
    return player_name.empty();
}

template <typename Request, typename Send>
std::optional<size_t> JoinToGameEmptyPlayerNameHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateJoinToGameEmptyPlayerNameResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool JoinToGameActivator(const Request& req) {
    return IsEqualUrls(api_urls::JOIN_TO_GAME_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> JoinToGameHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    auto parse_result = json_converter::ParseJoinToGameRequest(req.body());
    if (!parse_result) {
        return std::nullopt;
    }
    
    auto [player_name, map_id] = parse_result.value();
    if (application.FindMap(map_id) == nullptr) {
        return 0;
    }
    
    net::dispatch(*application.GetStrand(), [req, application = &application, send = std::forward<Send>(send)]() mutable {
        auto [player_name, map_id] = json_converter::ParseJoinToGameRequest(req.body()).value();
        auto [token, player_id] = application->JoinGame(player_name, map_id);
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreateJoinToGameResponse(*token, *player_id);
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
    });
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> JoinToGameMapNotFoundHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateJoinToGameMapNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> OnlyPostMethodAllowedHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::method_not_allowed, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.set(http::field::allow, "POST");
    response.body() = json_converter::CreateOnlyPostMethodAllowedResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool EmptyAuthorizationActivator(const Request& req) {
    if (GAME_API_URLS_WITH_AUTHORIZATION.count(req.target()) == 0) {
        return false;
    }
    auto auth_header = req[http::field::authorization];
    return auth_header.empty() || GetTokenString(auth_header).empty();
}

template <typename Request, typename Send>
std::optional<size_t> EmptyAuthorizationHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::unauthorized, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateEmptyAuthorizationResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool GetPlayersListActivator(const Request& req) {
    return IsEqualUrls(api_urls::GET_PLAYERS_LIST_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> GetPlayersListHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    authentication::Token token{GetTokenString(req[http::field::authorization])};
    if (!application.IsExistPlayer(token)) {
        return 0;
    }
    
    net::dispatch(*application.GetStrand(), [req, application = &application, send = std::forward<Send>(send)]() mutable {
        authentication::Token token{GetTokenString(req[http::field::authorization])};
        auto players = application->GetPlayersFromGameSession(token);
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreatePlayersListOnMapResponse(players);
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
    });
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> InvalidMethodHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::method_not_allowed, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.set(http::field::allow, "GET, HEAD");
    response.body() = json_converter::CreateInvalidMethodResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool GetGameStateActivator(const Request& req) {
    return IsEqualUrls(api_urls::GET_GAME_STATE_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> GetGameStateHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    authentication::Token token{GetTokenString(req[http::field::authorization])};
    if (!application.IsExistPlayer(token)) {
        return 0;
    }
    
    net::dispatch(*application.GetStrand(), [req, application = &application, send = std::forward<Send>(send)]() mutable {
        authentication::Token token{GetTokenString(req[http::field::authorization])};
        auto players = application->GetPlayersFromGameSession(token);
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreateGameStateResponse(players);
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
    });
    return std::nullopt;
}

template <typename Request>
bool InvalidContentTypeActivator(const Request& req) {
    if (GAME_API_URLS_WITH_JSON_REQ.count(req.target()) == 0) {
        return false;
    }
    auto content_type = req[http::field::content_type];
    return content_type.empty() || content_type != CONTENT_TYPE_APPLICATION_JSON;
}

template <typename Request, typename Send>
std::optional<size_t> InvalidContentTypeHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateInvalidContentTypeResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool PlayerActionInvalidActionActivator(const Request& req) {
    if (!IsEqualUrls(api_urls::MAKE_ACTION_API, req.target())) {
        return false;
    }
    auto res = json_converter::ParsePlayerActionRequest(req.body());
    if (!res.has_value()) {
        return false;
    }
    return !model::STRING_TO_DIRECTION.contains(res.value());
}

template <typename Request, typename Send>
std::optional<size_t> PlayerActionInvalidActionHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreatePlayerActionInvalidActionResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool PlayerActionActivator(const Request& req) {
    return IsEqualUrls(api_urls::MAKE_ACTION_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> PlayerActionHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    authentication::Token token{GetTokenString(req[http::field::authorization])};
    if (!application.IsExistPlayer(token)) {
        return 0;
    }
    
    net::dispatch(*application.GetStrand(), [req, application = &application, send = std::forward<Send>(send)]() mutable {
        authentication::Token token{GetTokenString(req[http::field::authorization])};
        std::string directionStr = json_converter::ParsePlayerActionRequest(req.body()).value();
        application->SetPlayerAction(token, model::STRING_TO_DIRECTION.at(directionStr));
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreatePlayerActionResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
    });
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> UnknownTokenHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::unauthorized, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateUnknownTokenResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> PageNotFoundHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.body() = json_converter::CreatePageNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool TimeTickInvalidMsgActivator(const Request& req) {
    if (!IsEqualUrls(api_urls::MAKE_TIME_TICK_API, req.target())) {
        return false;
    }
    auto res = json_converter::ParseSetDeltaTimeRequest(req.body());
    return !res.has_value();
}

template <typename Request, typename Send>
std::optional<size_t> TimeTickInvalidMsgHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    if (!application.IsManualTimeManagement()) {
        return 0;
    }
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateSetDeltaTimeInvalidMsgResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request>
bool TimeTickActivator(const Request& req) {
    return IsEqualUrls(api_urls::MAKE_TIME_TICK_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> TimeTickHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    if (!application.IsManualTimeManagement()) {
        return 0;
    }
    
    net::dispatch(*application.GetStrand(), [req, application = &application, send = std::forward<Send>(send)]() mutable {
        int delta_time = json_converter::ParseSetDeltaTimeRequest(req.body()).value();
        std::chrono::milliseconds dtime(delta_time);
        application->UpdateGameState(dtime);
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreateSetDeltaTimeResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
    });
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> InvalidEndpointHandler(
        const Request& req,
        app::Application& application,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateInvalidEndpointResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

}