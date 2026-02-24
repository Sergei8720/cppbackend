#include "json_converter.h"
#include "model_key_storage.h"
#include "json_key_storage.h"
#include <map>
#include <sstream>

namespace json_converter{

namespace json = boost::json;

std::string ConvertMapListToJson(const model::Game::Maps& maps) {
    json::array mapsArr;
    for (const auto& map : maps) {
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
    return "{}";
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
    
    for (const auto& item : players) {
        auto player = item.lock();
        if (!player) continue;
        
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
    
    for (const auto& item : players) {
        auto player = item.lock();
        if (!player) continue;
        
        auto dog = player->GetDog();
        if (!dog) continue;
        
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
    return "{}";
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
        
        std::string player_name = json::value_to<std::string>(obj.at(json_keys::REQUEST_PLAYER_NAME));
        std::string map_id_str = json::value_to<std::string>(obj.at(json_keys::REQUEST_MAP_ID));
        model::Map::Id map_id{map_id_str};
        
        return std::make_tuple(player_name, map_id);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> ParsePlayerActionRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        return json::value_to<std::string>(obj.at(json_keys::REQUEST_PLAYER_MOVE));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> ParseSetDeltaTimeRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        auto& time_delta = obj.at(json_keys::REQUEST_TIME_DELTA);
        
        if (!time_delta.is_int64()) {
            return std::nullopt;
        }
        
        return time_delta.as_int64();
    } catch (...) {
        return std::nullopt;
    }
}

}