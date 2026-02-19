#include "json_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

namespace {

std::string ReadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: "s + json_path.string());
    }
    
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

model::Point ParsePoint(const json::object& obj, const std::string& x_key, const std::string& y_key) {
    return {
        static_cast<model::Coord>(obj.at(x_key).as_int64()),
        static_cast<model::Coord>(obj.at(y_key).as_int64())
    };
}

void AddRoadsToMap(model::Map& map, const json::value& map_data) {
    if (!map_data.as_object().contains("roads")) {
        return;
    }
    
    for (const auto& road_data : map_data.as_object().at("roads").as_array()) {
        const auto& road_obj = road_data.as_object();
        auto start = ParsePoint(road_obj, "x0", "y0");
        
        if (road_obj.contains("x1")) {
            model::Coord end_x = static_cast<model::Coord>(road_obj.at("x1").as_int64());
            map.AddRoad(model::Road(start, {end_x, start.y}));
        } else {
            model::Coord end_y = static_cast<model::Coord>(road_obj.at("y1").as_int64());
            map.AddRoad(model::Road(start, {start.x, end_y}));
        }
    }
}

void AddBuildingsToMap(model::Map& map, const json::value& map_data) {
    if (!map_data.as_object().contains("buildings")) {
        return;
    }
    
    for (const auto& building_data : map_data.as_object().at("buildings").as_array()) {
        const auto& building_obj = building_data.as_object();
        
        model::Rectangle rect{
            ParsePoint(building_obj, "x", "y"),
            {
                static_cast<model::Dimension>(building_obj.at("w").as_int64()),
                static_cast<model::Dimension>(building_obj.at("h").as_int64())
            }
        };
        
        map.AddBuilding(model::Building(rect));
    }
}

void AddOfficesToMap(model::Map& map, const json::value& map_data) {
    if (!map_data.as_object().contains("offices")) {
        return;
    }
    
    for (const auto& office_data : map_data.as_object().at("offices").as_array()) {
        const auto& office_obj = office_data.as_object();
        
        model::Office::Id id(std::string(office_obj.at("id").as_string()));
        auto position = ParsePoint(office_obj, "x", "y");
        
        model::Offset offset{
            static_cast<model::Dimension>(office_obj.at("offsetX").as_int64()),
            static_cast<model::Dimension>(office_obj.at("offsetY").as_int64())
        };
        
        map.AddOffice(model::Office(std::move(id), position, offset));
    }
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::string content;
    try {
        content = ReadFile(json_path);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to read file '"s + json_path.string() + "': " + e.what());
    }
    
    json::value game_data;
    try {
        game_data = json::parse(content);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse JSON from file '"s + json_path.string() + "': " + e.what());
    }
    
    model::Game game;
    
    if (!game_data.as_object().contains("maps")) {
        return game;
    }
    
    for (const auto& map_data : game_data.as_object().at("maps").as_array()) {
        const auto& map_obj = map_data.as_object();
        
        model::Map::Id id(std::string(map_obj.at("id").as_string()));
        std::string name(map_obj.at("name").as_string());
        
        model::Map map(std::move(id), std::move(name));
        
        AddRoadsToMap(map, map_data);
        AddBuildingsToMap(map, map_data);
        AddOfficesToMap(map, map_data);
        
        game.AddMap(std::move(map));
    }
    
    return game;
}

}  // namespace json_loader