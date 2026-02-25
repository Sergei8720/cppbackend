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

std::optional<std::tuple<std::string, model::Map::Id>> ParseJoinToGameRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        
        if (!obj.contains(json_keys::REQUEST_PLAYER_NAME)) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Missing player name in request", "ParseJoinToGameRequest"));
            return std::nullopt;
        }
        if (!obj.contains(json_keys::REQUEST_MAP_ID)) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Missing map id in request", "ParseJoinToGameRequest"));
            return std::nullopt;
        }
        
        std::string player_name;
        try {
            player_name = json::value_to<std::string>(obj.at(json_keys::REQUEST_PLAYER_NAME));
        } catch (const json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Invalid player name type", e.what()));
            return std::nullopt;
        }
        
        std::string map_id_str;
        try {
            map_id_str = json::value_to<std::string>(obj.at(json_keys::REQUEST_MAP_ID));
        } catch (const json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Invalid map id type", e.what()));
            return std::nullopt;
        }
        
        model::Map::Id map_id{map_id_str};
        return std::make_tuple(player_name, map_id);
        
    } catch (const json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "JSON parse error", e.what()));
        return std::nullopt;
    } catch (const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Out of range error", e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Unexpected error in ParseJoinToGameRequest", e.what()));
        return std::nullopt;
    }
}

std::optional<std::string> ParsePlayerActionRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        
        if (!obj.contains(json_keys::REQUEST_PLAYER_MOVE)) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Missing move key in request", "ParsePlayerActionRequest"));
            return std::nullopt;
        }
        
        try {
            return json::value_to<std::string>(obj.at(json_keys::REQUEST_PLAYER_MOVE));
        } catch (const json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Invalid move value type", e.what()));
            return std::nullopt;
        }
        
    } catch (const json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "JSON parse error in player action", e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Unexpected error in ParsePlayerActionRequest", e.what()));
        return std::nullopt;
    }
}

std::optional<int> ParseSetDeltaTimeRequest(const std::string& msg) {
    try {
        json::value jv = json::parse(msg);
        auto& obj = jv.as_object();
        
        if (!obj.contains(json_keys::REQUEST_TIME_DELTA)) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Missing timeDelta key", "ParseSetDeltaTimeRequest"));
            return std::nullopt;
        }
        
        auto& time_delta = obj.at(json_keys::REQUEST_TIME_DELTA);
        if (!time_delta.is_int64()) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "timeDelta is not integer", 
                    "Type: " + std::string(time_delta.kind() == json::kind::uint64 ? "uint64" : 
                                           time_delta.kind() == json::kind::int64 ? "int64" : "other")));
            return std::nullopt;
        }
        
        int64_t value = time_delta.as_int64();
        if (value < 0) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Negative timeDelta value", std::to_string(value)));
        }
        
        return static_cast<int>(value);
        
    } catch (const json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "JSON parse error in time delta", e.what()));
        return std::nullopt;
    } catch (const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Out of range error in time delta", e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Unexpected error in ParseSetDeltaTimeRequest", e.what()));
        return std::nullopt;
    }
}

}