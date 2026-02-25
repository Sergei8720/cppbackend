#include "json_loader.h"
#include "logger.h"
#include "json_key_storage.h"
#include "model_key_storage.h"

#include <fstream>
#include <iostream>
#include <string_view>
#include <sstream>

namespace json_loader {

using namespace std::literals;

boost::json::value ReadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        std::string error_msg = "Failed to open file: " + json_path.string();
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(EXIT_FAILURE, error_msg, "ReadFile"));
        throw OpenConfigFileOfModelException();
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    
    try {
        return boost::json::parse(ss.str());
    } catch (const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(EXIT_FAILURE, "JSON syntax error", e.what()));
        throw;
    } catch (const std::exception& e) {
        // Другие ошибки
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(EXIT_FAILURE, "Failed to parse JSON", e.what()));
        throw;
    }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    model::Game game;
    
    try {
        boost::json::value jsonVal = ReadFile(json_path);
        
        if (!jsonVal.as_object().contains(model::MAPS)) {
            throw std::runtime_error("Missing 'maps' key in config file");
        }
        
        try {
            std::vector<model::Map> maps = boost::json::value_to< std::vector<model::Map> >(
                jsonVal.as_object().at(model::MAPS));
            game.AddMaps(maps);
            BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("info"sv,
                logware::ExceptionLogData(0, "Successfully loaded maps", 
                    "count: " + std::to_string(maps.size())));
        } catch (const boost::json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(EXIT_FAILURE, "Failed to convert maps from JSON", e.what()));
            throw;
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(EXIT_FAILURE, "Error adding maps to game", e.what()));
            throw;
        }
        
        try {
            if (jsonVal.as_object().contains(model::DEFAULT_DOG_VELOCITY)) {
                auto& velocity_val = jsonVal.as_object().at(model::DEFAULT_DOG_VELOCITY);
                
                if (!velocity_val.is_double() && !velocity_val.is_int64()) {
                    BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                        logware::ExceptionLogData(0, "defaultDogSpeed has invalid type", 
                            std::string(velocity_val.kind() == json::kind::double_ ? "double" : 
                                       velocity_val.kind() == json::kind::int64 ? "int64" : "other")));
                } else {
                    double default_dog_velocity = json::value_to<double>(velocity_val);
                    if (default_dog_velocity <= 0) {
                        BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                            logware::ExceptionLogData(0, "defaultDogSpeed is not positive", 
                                std::to_string(default_dog_velocity)));
                    }
                    game.SetDefaultDogVelocity(default_dog_velocity);
                    BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("info"sv,
                        logware::ExceptionLogData(0, "Default dog velocity set", 
                            std::to_string(default_dog_velocity)));
                }
            }
        } catch (const boost::json::system_error& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Failed to parse defaultDogSpeed", e.what()));
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Unexpected error parsing defaultDogSpeed", e.what()));
        }
        
    } catch (const OpenConfigFileOfModelException& e) {
        throw;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(EXIT_FAILURE, "Failed to load game", e.what()));
        throw;
    }
    
    return game;
}

}