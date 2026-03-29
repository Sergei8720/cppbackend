#pragma once
#include "application.h"
#include "player_tokens.h"
#include "json_converter.h"
#include "request_handlers_utils.h"
#include "api_url_storage.h"
#include "database/database.h"

#include <vector>
#include <optional>
#include <variant>
#include <chrono>
#include <unordered_set>
#include <boost/beast/http.hpp>
#include <boost/thread/future.h>

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
    return !url.empty() &&
            url[0] == "api" &&
            (
                url.size() > SIZE_OF_FIVE_SEGMENT_URL ||
                url.size() < SIZE_OF_THREE_SEGMENT_URL ||
                (url.size() >= SIZE_OF_TWO_SEGMENT_URL && 
                    url[1] != "v1") ||
                (url.size() >= SIZE_OF_THREE_SEGMENT_URL &&
                    url[2] != "maps" &&
                    url[2] != "game" &&
                    url[3] != "join" &&
                    url[3] != "players" &&
                    url[3] != "state" &&
                    url[3] != "player" &&
                    url[3] != "tick" &&
                    url[3] != "records" &&
                    (url.size() == SIZE_OF_FIVE_SEGMENT_URL && url[4] != "action"))
            );
};

template <typename Request, typename Send>
std::optional<size_t> BadRequestHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.body() = json_converter::CreateBadRequestResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
};


template <typename Request>
bool GetMapListActivator(const Request& req) {
    return IsEqualUrls(api_urls::GET_MAPS_LIST_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> GetMapListHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
        Send&& send) {
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::ConvertMapListToJson(application->ListMap());
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
        std::shared_ptr<app::Application> application,
        Send&& send) {
    auto id = SplitUrl(req.target())[3];
    auto map = application->FindMap(model::Map::Id(std::string(id)));
    if(map == nullptr) {
        return 0;
    }
    http::response<http::string_body> response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::ConvertMapToJson(*map);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
};

template <typename Request, typename Send>
std::optional<size_t> MapNotFoundHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
        Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateMapNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
};


template <typename Request>
bool JoinToGameInvalidJsonReqActivator(const Request& req) {
    return IsEqualUrls(api_urls::JOIN_TO_GAME_API, req.target()) &&
        !json_converter::ParseJoinToGameRequest(req.body());
}

template <typename Request, typename Send>
std::optional<size_t> JoinToGameInvalidJsonReqHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
    if(IsEqualUrls(api_urls::JOIN_TO_GAME_API, req.target())) {
        auto res = json_converter::ParseJoinToGameRequest(req.body());
        if(!res) {
            return false;
        }
        auto [player_name, map_id] = res.value();
        return player_name.empty();
    }
    return false;
}

template <typename Request, typename Send>
std::optional<size_t> JoinToGameEmptyPlayerNameHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
        std::shared_ptr<app::Application> application,
        Send&& send) {
    auto [player_name, map_id] = json_converter::ParseJoinToGameRequest(req.body()).value();
    if(application->FindMap(map_id) == nullptr) {
        return 0;
    }
    StringResponse response(http::status::ok, req.version());
    auto session = application->FindGameSessionBy(map_id);
    if(session) {
        boost::promise<std::string> res_promise;
        auto res_future = res_promise.get_future();
        net::dispatch(*(session->GetStrand()),
            [&res_promise
            , application
            , &player_name
            , &map_id] {
            auto [token, player_id] = application->JoinGame(player_name, map_id);
            res_promise.set_value(json_converter::CreateJoinToGameResponse(*token, *player_id));
        });
        response.body() = res_future.get();
    } else {
        auto [token, player_id] = application->JoinGame(player_name, map_id);
        response.body() = json_converter::CreateJoinToGameResponse(*token, *player_id);
    }
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> JoinToGameMapNotFoundHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
        std::shared_ptr<app::Application> application,
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
};


template <typename Request>
bool EmptyAuthorizationActivator(const Request& req) {
    return (GAME_API_URLS_WITH_AUTHORIZATION.count(req.target()) > 0) &&
            (req[http::field::authorization].empty() ||
            GetTokenString(req[http::field::authorization]).empty());
}

template <typename Request, typename Send>
std::optional<size_t> EmptyAuthorizationHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
        std::shared_ptr<app::Application> application,
        Send&& send) {
    authentication::Token token{GetTokenString(req[http::field::authorization])};
    if(!application->IsExistPlayer(token)) {
        return 0;
    }
    boost::promise<std::variant<std::string, size_t>> res_promise;
    auto res_future = res_promise.get_future();
    net::dispatch(*(application->FindGameSessionBy(token)->GetStrand()),
        [&res_promise
        , &token
        , application]{
        auto players = application->GetPlayersFromGameSession(token);
        res_promise.set_value(json_converter::CreatePlayersListOnMapResponse(players));
    });
    auto res = res_future.get();
    if(std::holds_alternative<size_t>(res)){
        return std::get<size_t>(res);
    }
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = std::get<std::string>(res);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> InvalidMethodHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
};


template <typename Request>
bool GetGameStateActivator(const Request& req) {
    return IsEqualUrls(api_urls::GET_GAME_STATE_API, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> GetGameStateHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
        Send&& send) {
    authentication::Token token{GetTokenString(req[http::field::authorization])};
    if(!application->IsExistPlayer(token)) {
        return 0;
    }
    boost::promise<std::variant<std::string, size_t>> res_promise;
    auto res_future = res_promise.get_future();
    net::dispatch(*(application->FindGameSessionBy(token)->GetStrand()),
        [&res_promise
        , &token
        , application] {
        auto players = application->GetPlayersFromGameSession(token);
        res_promise.set_value(json_converter::CreateGameStateResponse(
            players,
            application->FindGameSessionBy(token)->GetLostObjects())
        );
    });
    auto res = res_future.get();
    if(std::holds_alternative<size_t>(res)){
        return std::get<size_t>(res);
    }
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = std::get<std::string>(res);
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}


template <typename Request>
bool InvalidContentTypeActivator(const Request& req) {
    return (GAME_API_URLS_WITH_JSON_REQ.count(req.target()) > 0) &&
            (req[http::field::content_type].empty() ||
            req[http::field::content_type] != CONTENT_TYPE_APPLICATION_JSON);
}

template <typename Request, typename Send>
std::optional<size_t> InvalidContentTypeHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
    if(IsEqualUrls(api_urls::MAKE_ACTION_API, req.target())) {
        auto res = json_converter::ParsePlayerActionRequest(req.body());
        if(res.has_value()) {
            return !model::STRING_TO_DIRECTION.contains(res.value());
        }
    }
    return false;
}

template <typename Request, typename Send>
std::optional<size_t> PlayerActionInvalidActionHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
        std::shared_ptr<app::Application> application,
        Send&& send) {
    authentication::Token token{GetTokenString(req[http::field::authorization])};
    if(!application->IsExistPlayer(token)) {
        return 0;
    }
    boost::promise<std::optional<size_t>> res_promise;
    auto res_future = res_promise.get_future();
    net::dispatch(*(application->FindGameSessionBy(token)->GetStrand()),
        [&res_promise
        , &token
        , &req
        , application]{
        std::string directionStr = json_converter::ParsePlayerActionRequest(req.body()).value();
        application->SetPlayerAction(token, model::STRING_TO_DIRECTION.at(directionStr));
        res_promise.set_value(std::nullopt);
    });
    auto res = res_future.get();
    if(res) {
        return res;
    }
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreatePlayerActionResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return res;
}


template <typename Request, typename Send>
std::optional<size_t> UnknownTokenHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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
        std::shared_ptr<app::Application> application,
        Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.body() = json_converter::CreatePageNotFoundResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
};


template <typename Request>
bool TimeTickInvalidMsgActivator(const Request& req) {
    if(IsEqualUrls(api_urls::MAKE_TIME_TICK_API, req.target())) {
        auto res = json_converter::ParseSetDeltaTimeRequest(req.body());
        return !res.has_value();
    }
    return false;
}

template <typename Request, typename Send>
std::optional<size_t> TimeTickInvalidMsgHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
        Send&& send) {
    if(!application->IsManualTimeManagement()) {
        StringResponse response(http::status::bad_request, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreateBadRequestResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
        return std::nullopt;
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
        std::shared_ptr<app::Application> application,
        Send&& send) {
    if(!application->IsManualTimeManagement()) {
        StringResponse response(http::status::bad_request, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreateBadRequestResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
        return std::nullopt;
    }
    std::chrono::milliseconds dtime(json_converter::ParseSetDeltaTimeRequest(req.body()).value());
    application->UpdateGameState(dtime);
    StringResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
    response.set(http::field::cache_control, NO_CACHE_CONTROL);
    response.body() = json_converter::CreateSetDeltaTimeResponse();
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
    return std::nullopt;
}

template <typename Request, typename Send>
std::optional<size_t> InvalidEndpointHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
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


// ============ GET /api/v1/game/records ============

template <typename Request>
bool GetRecordsActivator(const Request& req) {
    auto url = SplitUrl(req.target());
    std::string base_url = api_urls::GET_RECORDS_API;
    // Проверяем URL без параметров
    if (req.target().find('?') != std::string::npos) {
        std::string target_without_query = std::string(req.target().substr(0, req.target().find('?')));
        return IsEqualUrls(base_url, target_without_query);
    }
    return IsEqualUrls(base_url, req.target());
}

template <typename Request, typename Send>
std::optional<size_t> GetRecordsHandler(
        const Request& req,
        std::shared_ptr<app::Application> application,
        Send&& send) {
    
    // Парсим параметры запроса start и maxItems
    int start = 0;
    int maxItems = 100;
    
    std::string target(req.target());
    auto question_pos = target.find('?');
    if (question_pos != std::string::npos) {
        std::string query = target.substr(question_pos + 1);
        std::stringstream ss(query);
        std::string param;
        while (std::getline(ss, param, '&')) {
            auto eq_pos = param.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = param.substr(0, eq_pos);
                std::string value = param.substr(eq_pos + 1);
                if (key == "start") {
                    try {
                        start = std::stoi(value);
                        if (start < 0) start = 0;
                    } catch (...) {}
                } else if (key == "maxItems") {
                    try {
                        maxItems = std::stoi(value);
                        if (maxItems < 0) maxItems = 0;
                    } catch (...) {}
                }
            }
        }
    }
    
    // Проверка лимита maxItems
    if (maxItems > 100) {
        StringResponse response(http::status::bad_request, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreateBadRequestResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
        return std::nullopt;
    }
    
    // Получаем рекорды из БД
    auto pool = application->GetConnectionPool();
    if (!pool) {
        // Если нет БД, возвращаем пустой массив
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = "[]";
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
        return std::nullopt;
    }
    
    try {
        auto records = database::Database::GetRecords(pool, start, maxItems);
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::ConvertRecordsToJson(records);
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
    } catch (const std::exception& e) {
        StringResponse response(http::status::internal_server_error, req.version());
        response.set(http::field::content_type, CONTENT_TYPE_APPLICATION_JSON);
        response.set(http::field::cache_control, NO_CACHE_CONTROL);
        response.body() = json_converter::CreateInternalServerErrorResponse();
        response.content_length(response.body().size());
        response.keep_alive(req.keep_alive());
        send(response);
    }
    return std::nullopt;
}


// ============ КЛАСС EXECUTOR ============

template<typename Request, typename Send>
class ApiV1RequestHandlerExecutor{
    using ActivatorType = bool(*)(const Request&);
    using HandlerType = std::optional<size_t>(*)(const Request&, std::shared_ptr<app::Application>, Send&&);
public:
    // убираем конструктор копирования
    ApiV1RequestHandlerExecutor(const ApiV1RequestHandlerExecutor&) = delete;
    ApiV1RequestHandlerExecutor& operator=(const ApiV1RequestHandlerExecutor&) = delete;
    ApiV1RequestHandlerExecutor(ApiV1RequestHandlerExecutor&&) = delete;
    ApiV1RequestHandlerExecutor& operator=(ApiV1RequestHandlerExecutor&&) = delete;

    // получение ссылки на единственный объект
    static ApiV1RequestHandlerExecutor& GetInstance() {
        static ApiV1RequestHandlerExecutor obj;
        return obj;
    };

    bool Execute(const Request& req, std::shared_ptr<app::Application> application, Send&& send) {
        for(auto item : rh_storage_) {
            if(item.GetActivator()(req)){
                    auto res = item.GetHandler(req.method())(req, application, std::forward<Send>(send));
                    while(res.has_value()){
                        res = item.GetEmergeHandlerByIndex(res.value())(req, application, std::forward<Send>(send));
                    }
                return true;
            }
        }
        return false;
    };

private:
    
    std::vector< RequestHandlerNode<ActivatorType, HandlerType> > rh_storage_ = {
        RequestHandlerNode<ActivatorType, HandlerType>(BadRequestActivator,
                                                        {{http::verb::get, BadRequestHandler}},
                                                        BadRequestHandler),

        RequestHandlerNode<ActivatorType, HandlerType>(GetMapListActivator,
                                                        {{http::verb::get, GetMapListHandler}},
                                                        BadRequestHandler),

        RequestHandlerNode<ActivatorType, HandlerType>(GetMapByIdActivator,
                                                        {{http::verb::get, GetMapByIdHandler},
                                                        {http::verb::head, GetMapByIdHandler}},
                                                        InvalidMethodHandler,
                                                        {MapNotFoundHandler}),
                                                        
        RequestHandlerNode<ActivatorType, HandlerType>(InvalidContentTypeActivator,
                                                        {{http::verb::post, InvalidContentTypeHandler}},
                                                        InvalidContentTypeHandler),

        RequestHandlerNode<ActivatorType, HandlerType>(JoinToGameInvalidJsonReqActivator,
                                                        {{http::verb::post, JoinToGameInvalidJsonReqHandler}},
                                                        OnlyPostMethodAllowedHandler),
        RequestHandlerNode<ActivatorType, HandlerType>(JoinToGameEmptyPlayerNameActivator,
                                                        {{http::verb::post, JoinToGameEmptyPlayerNameHandler}},
                                                        OnlyPostMethodAllowedHandler),
        RequestHandlerNode<ActivatorType, HandlerType>(JoinToGameActivator,
                                                        {{http::verb::post, JoinToGameHandler}},
                                                        OnlyPostMethodAllowedHandler,
                                                        {JoinToGameMapNotFoundHandler}),

        RequestHandlerNode<ActivatorType, HandlerType>(EmptyAuthorizationActivator,
                                                        {{http::verb::get, EmptyAuthorizationHandler},
                                                        {http::verb::head, EmptyAuthorizationHandler}},
                                                        InvalidMethodHandler),

        RequestHandlerNode<ActivatorType, HandlerType>(GetPlayersListActivator,
                                                        {{http::verb::get, GetPlayersListHandler},
                                                        {http::verb::head, GetPlayersListHandler}},
                                                        InvalidMethodHandler,
                                                        {UnknownTokenHandler}),
        RequestHandlerNode<ActivatorType, HandlerType>(GetGameStateActivator,
                                                        {{http::verb::get, GetGameStateHandler},
                                                        {http::verb::head, GetGameStateHandler}},
                                                        InvalidMethodHandler,
                                                        {UnknownTokenHandler}),

        RequestHandlerNode<ActivatorType, HandlerType>(PlayerActionInvalidActionActivator,
                                                        {{http::verb::post, PlayerActionInvalidActionHandler}},
                                                        OnlyPostMethodAllowedHandler),
        RequestHandlerNode<ActivatorType, HandlerType>(PlayerActionActivator,
                                                        {{http::verb::post, PlayerActionHandler}},
                                                        InvalidMethodHandler,
                                                        {UnknownTokenHandler}),
        
        RequestHandlerNode<ActivatorType, HandlerType>(TimeTickInvalidMsgActivator,
                                                        {{http::verb::post, TimeTickInvalidMsgHandler}},
                                                        InvalidMethodHandler,
                                                        {InvalidEndpointHandler}),
        RequestHandlerNode<ActivatorType, HandlerType>(TimeTickActivator,
                                                        {{http::verb::post, TimeTickHandler}},
                                                        InvalidMethodHandler,
                                                        {InvalidEndpointHandler}),
        
        // НОВЫЙ УЗЕЛ ДЛЯ /api/v1/game/records
        RequestHandlerNode<ActivatorType, HandlerType>(GetRecordsActivator,
                                                        {{http::verb::get, GetRecordsHandler},
                                                         {http::verb::head, GetRecordsHandler}},
                                                        InvalidMethodHandler)
    };

    ApiV1RequestHandlerExecutor() = default;
};

}