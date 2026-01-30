#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace json_loader {

namespace {

namespace json_keys {
    constexpr const char* MAPS = "maps";
    constexpr const char* ID = "id";
    constexpr const char* NAME = "name";
    constexpr const char* ROADS = "roads";
    constexpr const char* BUILDINGS = "buildings";
    constexpr const char* OFFICES = "offices";
    constexpr const char* X0 = "x0";
    constexpr const char* Y0 = "y0";
    constexpr const char* X1 = "x1";
    constexpr const char* Y1 = "y1";
    constexpr const char* X = "x";
    constexpr const char* Y = "y";
    constexpr const char* WIDTH = "width";
    constexpr const char* HEIGHT = "height";
    constexpr const char* OFFSET_X = "offsetX";
    constexpr const char* OFFSET_Y = "offsetY";
}

std::string ReadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + json_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

model::Point ParsePoint(const json::value& value, const std::string& x_key, const std::string& y_key) {
    double x = value.at(x_key).as_double();
    double y = value.at(y_key).as_double();
    return model::Point{x, y};
}

model::Road ParseRoad(const json::value& value) {
    if (value.as_object().contains(json_keys::X0)) {
        double x0 = value.at(json_keys::X0).as_double();
        double x1 = value.at(json_keys::X1).as_double();
        double y = value.at(json_keys::Y).as_double();
        return model::Road{model::Road::HORIZONTAL, 
                          model::Point{x0, y}, 
                          model::Point{x1, y}};
    } else {
        double x = value.at(json_keys::X).as_double();
        double y0 = value.at(json_keys::Y0).as_double();
        double y1 = value.at(json_keys::Y1).as_double();
        return model::Road{model::Road::VERTICAL, 
                          model::Point{x, y0}, 
                          model::Point{x, y1}};
    }
}

model::Building ParseBuilding(const json::value& value) {
    double x = value.at(json_keys::X).as_double();
    double y = value.at(json_keys::Y).as_double();
    double width = value.at(json_keys::WIDTH).as_double();
    double height = value.at(json_keys::HEIGHT).as_double();
    
    model::Rectangle bounds{{x, y}, {width, height}};
    return model::Building{bounds};
}

model::Office ParseOffice(const json::value& value) {
    using namespace std::literals;
    
    std::string id = value.at(json_keys::ID).as_string();
    model::Point position = ParsePoint(value, json_keys::X, json_keys::Y);
    model::Offset offset{value.at(json_keys::OFFSET_X).as_double(),
                         value.at(json_keys::OFFSET_Y).as_double()};
    
    return model::Office{model::Office::Id{id}, position, offset};
}

model::Map ParseMap(const json::value& value) {
    using namespace std::literals;
    
    std::string id = value.at(json_keys::ID).as_string();
    std::string name = value.at(json_keys::NAME).as_string();
    
    model::Map map{model::Map::Id{id}, name};
    
    const auto& roads = value.at(json_keys::ROADS).as_array();
    for (const auto& road_value : roads) {
        map.AddRoad(ParseRoad(road_value));
    }
    
    const auto& buildings = value.at(json_keys::BUILDINGS).as_array();
    for (const auto& building_value : buildings) {
        map.AddBuilding(ParseBuilding(building_value));
    }
    
    const auto& offices = value.at(json_keys::OFFICES).as_array();
    for (const auto& office_value : offices) {
        map.AddOffice(ParseOffice(office_value));
    }
    
    return map;
}

model::Game ParseGame(const json::value& value) {
    model::Game game;
    
    const auto& maps = value.at(json_keys::MAPS).as_array();
    for (const auto& map_value : maps) {
        game.AddMap(ParseMap(map_value));
    }
    
    return game;
}

bool ValidateGameJson(const json::value& data) {
    try {
        if (!data.is_object()) {
            return false;
        }
        
        const auto& obj = data.as_object();
        
        if (!obj.contains(json_keys::MAPS)) {
            return false;
        }
        
        const auto& maps = obj.at(json_keys::MAPS);
        if (!maps.is_array()) {
            return false;
        }
        
        const auto& maps_array = maps.as_array();
        for (const auto& map_value : maps_array) {
            if (!map_value.is_object()) {
                return false;
            }
            
            const auto& map_obj = map_value.as_object();
            if (!map_obj.contains(json_keys::ID) || 
                !map_obj.contains(json_keys::NAME) ||
                !map_obj.contains(json_keys::ROADS) ||
                !map_obj.contains(json_keys::BUILDINGS) ||
                !map_obj.contains(json_keys::OFFICES)) {
                return false;
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    try {
        std::string content = ReadFile(json_path);
        
        json::value game_data = json::parse(content);
        
        if (!ValidateGameJson(game_data)) {
            throw std::runtime_error("Invalid game JSON structure in file: " + 
                                     json_path.string());
        }
        
        return ParseGame(game_data);
        
    } catch (const json::parse_error& e) {
        std::stringstream ss;
        ss << "JSON parse error at byte " << e.byte
           << " in file '" << json_path.string() << "': " << e.what();
        std::cerr << "ERROR: " << ss.str() << std::endl;
        throw std::runtime_error(ss.str());
        
    } catch (const json::exception& e) {
        std::string error_msg = "JSON error in file '" + 
                               json_path.string() + "': " + e.what();
        std::cerr << "ERROR: " << error_msg << std::endl;
        throw std::runtime_error(error_msg);
        
    } catch (const std::runtime_error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        throw;
        
    } catch (const std::exception& e) {
        std::string error_msg = "Error loading game from '" + 
                               json_path.string() + "': " + e.what();
        std::cerr << "ERROR: " << error_msg << std::endl;
        throw std::runtime_error(error_msg);
    }
}

}  // namespace json_loader