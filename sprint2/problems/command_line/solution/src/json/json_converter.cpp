#include "json_converter.h"
#include "model_key_storage.h"
#include "json_key_storage.h"
#include "logger.h"

#include <map>
#include <sstream>
#include <boost/json/array.hpp>
#include <boost/json.hpp>

namespace json_converter{

namespace json = boost::json;
using namespace std::literals;

std::string ConvertMapListToJson(const model::Game::Maps& maps) {
    json::array mapsArr;
    for(const auto& map : maps) {
        json::object item;
        item[model::MAP_ID] = *(map->GetId());
        item[model::MAP_NAME] = map->GetName();
        mapsArr.push_back(item);
    }
    return json::serialize(mapsArr);
}

std::string ConvertMapToJson(const model::Map& map) {
    return json::serialize(json::value_from(map));
}

std::string CreateMapNotFoundResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "mapNotFound";
    msg[json_keys::RESPONSE_MESSAGE] = "Map not found";
    return json::serialize(msg);
}

std::string CreateBadRequestResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "badRequest";
    msg[json_keys::RESPONSE_MESSAGE] = "Bad request";
    return json::serialize(msg);
}

std::string CreatePageNotFoundResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "pageNotFound";
    msg[json_keys::RESPONSE_MESSAGE] = "Page not found";
    return json::serialize(msg);
}

std::string CreateOnlyPostMethodAllowedResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidMethod";
    msg[json_keys::RESPONSE_MESSAGE] = "Only POST method is expected";
    return json::serialize(msg);
}

std::string CreateJoinToGameInvalidArgumentResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidArgument";
    msg[json_keys::RESPONSE_MESSAGE] = "Join game request parse error";
    return json::serialize(msg);
}

std::string CreateJoinToGameMapNotFoundResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "mapNotFound";
    msg[json_keys::RESPONSE_MESSAGE] = "Map not found";
    return json::serialize(msg);
}

std::string CreateJoinToGameEmptyPlayerNameResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidArgument";
    msg[json_keys::RESPONSE_MESSAGE] = "Invalid name";
    return json::serialize(msg);
}

std::string CreateInvalidMethodResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidMethod";
    msg[json_keys::RESPONSE_MESSAGE] = "Invalid method";
    return json::serialize(msg);
}

std::string CreateEmptyAuthorizationResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidToken";
    msg[json_keys::RESPONSE_MESSAGE] = "Authorization header is required";
    return json::serialize(msg);
}

std::string CreateUnknownTokenResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "unknownToken";
    msg[json_keys::RESPONSE_MESSAGE] = "Player token has not been found";
    return json::serialize(msg);
}

std::string CreatePlayerActionResponse() {
    json::object msg;
    return json::serialize(msg);
}

std::string CreatePlayerActionInvalidActionResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidArgument";
    msg[json_keys::RESPONSE_MESSAGE] = "Failed to parse action";
    return json::serialize(msg);
}

std::string CreateInvalidContentTypeResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidArgument";
    msg[json_keys::RESPONSE_MESSAGE] = "Invalid content type";
    return json::serialize(msg);
}

std::string CreatePlayersListOnMapResponse(const std::vector<std::weak_ptr<app::Player>>& players) {
    json::object obj;
    
    for(const auto& item : players) {
        auto player = item.lock();
        if(!player) continue;
        
        std::stringstream ss;
        ss << *(player->GetId());
        
        json::object player_obj;
        player_obj[json_keys::RESPONSE_PLAYER_NAME] = player->GetName();
        obj[ss.str()] = player_obj;
    }
    
    return json::serialize(obj);
}

std::string CreateGameStateResponse(const std::vector<std::weak_ptr<app::Player>>& players) {
    json::object players_obj;
    
    for(const auto& item : players) {
        auto player = item.lock();
        if(!player) continue;
        
        auto dog = player->GetDog();
        if(!dog) continue;
        
        std::stringstream ss;
        ss << *(player->GetId());
        
        json::array pos = {dog->GetPosition().x, dog->GetPosition().y};
        json::array speed = {dog->GetVelocity().vx, dog->GetVelocity().vy};
        
        json::object player_obj;
        player_obj[json_keys::RESPONSE_DOG_POSITION] = pos;
        player_obj[json_keys::RESPONSE_DOG_VELOCITY] = speed;
        player_obj[json_keys::RESPONSE_DOG_DIRECTION] = model::DIRECTION_TO_STRING.at(dog->GetDirection());
        
        players_obj[ss.str()] = player_obj;
    }
    
    json::object result;
    result[json_keys::RESPONSE_PLAYERS] = players_obj;
    return json::serialize(result);
}

std::string CreateJoinToGameResponse(const std::string& token, size_t player_id) {
    json::object msg;
    msg[json_keys::RESPONSE_AUTHORISATION_TOKEN] = token;
    msg[json_keys::RESPONSE_PLAYER_ID] = player_id;
    return json::serialize(msg);
}

std::string CreateSetDeltaTimeResponse() {
    json::object msg;
    return json::serialize(msg);
}

std::string CreateSetDeltaTimeInvalidMsgResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "invalidArgument";
    msg[json_keys::RESPONSE_MESSAGE] = "Failed to parse tick request JSON";
    return json::serialize(msg);
}

std::string CreateInvalidEndpointResponse() {
    json::object msg;
    msg[json_keys::RESPONSE_CODE] = "badRequest";
    msg[json_keys::RESPONSE_MESSAGE] = "Invalid endpoint";
    return json::serialize(msg);
}

std::optional<std::tuple<std::string, model::Map::Id>> ParseJoinToGameRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        
        if(!obj.contains(json_keys::REQUEST_PLAYER_NAME) || !obj.contains(json_keys::REQUEST_MAP_ID)) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Missing required fields in join request", "ParseJoinToGameRequest"));
            return std::nullopt;
        }
        
        std::string player_name;
        try {
            player_name = json::value_to<std::string>(obj.at(json_keys::REQUEST_PLAYER_NAME));
        } catch(const json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Invalid player name type", e.what()));
            return std::nullopt;
        }
        
        std::string map_id_str;
        try {
            map_id_str = json::value_to<std::string>(obj.at(json_keys::REQUEST_MAP_ID));
        } catch(const json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Invalid map id type", e.what()));
            return std::nullopt;
        }
        
        model::Map::Id map_id{map_id_str};
        return std::make_tuple(player_name, map_id);
        
    } catch(const json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "JSON parse error in join request", e.what()));
        return std::nullopt;
    } catch(const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Unexpected error parsing join request", e.what()));
        return std::nullopt;
    }
}

std::optional<std::string> ParsePlayerActionRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        
        if(!obj.contains(json_keys::REQUEST_PLAYER_MOVE)) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Missing move field in player action request", "ParsePlayerActionRequest"));
            return std::nullopt;
        }
        
        try {
            return json::value_to<std::string>(obj.at(json_keys::REQUEST_PLAYER_MOVE));
        } catch(const json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Invalid move value type", e.what()));
            return std::nullopt;
        }
        
    } catch(const json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "JSON parse error in player action", e.what()));
        return std::nullopt;
    } catch(const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Unexpected error parsing player action", e.what()));
        return std::nullopt;
    }
}

std::optional<int> ParseSetDeltaTimeRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        
        if(!obj.contains(json_keys::REQUEST_TIME_DELTA)) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Missing timeDelta field", "ParseSetDeltaTimeRequest"));
            return std::nullopt;
        }
        
        auto& time_delta = obj.at(json_keys::REQUEST_TIME_DELTA);
        if(!time_delta.is_int64()) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "timeDelta is not integer", 
                    "type: " + std::string(time_delta.kind() == json::kind::int64 ? "int64" : 
                                          time_delta.kind() == json::kind::uint64 ? "uint64" : "other")));
            return std::nullopt;
        }
        
        int64_t value = time_delta.as_int64();
        if(value < 0) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Negative timeDelta value", std::to_string(value)));
        }
        
        return static_cast<int>(value);
        
    } catch(const json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "JSON parse error in time delta", e.what()));
        return std::nullopt;
    } catch(const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Unexpected error parsing time delta", e.what()));
        return std::nullopt;
    }
}

}