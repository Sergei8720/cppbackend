#include "json_loader.h"

#include <fstream>
#include <string>
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;

namespace {

model::Coord GetCoord(const json::object& obj, const std::string& key) {
    return static_cast<model::Coord>(obj.at(key).as_int64());
}

std::string ReadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

void AddRoadsToMap(model::Map& map, const json::value& map_data) {
    if (!map_data.as_object().contains("roads")) {
        return;
    }
    
    const auto& roads = map_data.as_object().at("roads").as_array();
    
    for (const auto& road_item : roads) {
        const auto& road_obj = road_item.as_object();
        model::Point start{GetCoord(road_obj, "x0"), GetCoord(road_obj, "y0")};
        
        if (road_obj.contains("x1")) {
            model::Coord end_x = GetCoord(road_obj, "x1");
            map.AddRoad(model::Road(model::Road::Orientation::HORIZONTAL, start, end_x));
        } else {
            model::Coord end_y = GetCoord(road_obj, "y1");
            map.AddRoad(model::Road(model::Road::Orientation::VERTICAL, start, end_y));
        }
    }
}

void AddBuildingsToMap(model::Map& map, const json::value& map_data) {
    if (!map_data.as_object().contains("buildings")) {
        return;
    }
    
    const auto& buildings = map_data.as_object().at("buildings").as_array();
    
    for (const auto& building_item : buildings) {
        const auto& building_obj = building_item.as_object();
        model::Rectangle rect{
            {GetCoord(building_obj, "x"), GetCoord(building_obj, "y")},
            {GetCoord(building_obj, "w"), GetCoord(building_obj, "h")}
        };
        map.AddBuilding(model::Building(rect));
    }
}

void AddOfficesToMap(model::Map& map, const json::value& map_data) {
    if (!map_data.as_object().contains("offices")) {
        return;
    }
    
    const auto& offices = map_data.as_object().at("offices").as_array();
    
    for (const auto& office_item : offices) {
        const auto& office_obj = office_item.as_object();
        model::Office::Id id{std::string(office_obj.at("id").as_string())};
        model::Point position{GetCoord(office_obj, "x"), GetCoord(office_obj, "y")};
        model::Offset offset{GetCoord(office_obj, "offsetX"), GetCoord(office_obj, "offsetY")};
        
        map.AddOffice(model::Office(std::move(id), position, offset));
    }
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::string json_content = ReadFile(json_path);
    json::value game_json = json::parse(json_content);
    
    model::Game game;
    const auto& maps_array = game_json.as_object().at("maps").as_array();
    
    for (const auto& map_item : maps_array) {
        const auto& map_obj = map_item.as_object();
        model::Map::Id id{std::string(map_obj.at("id").as_string())};
        std::string name{map_obj.at("name").as_string()};
        
        model::Map map(std::move(id), std::move(name));
        
        AddRoadsToMap(map, map_item);
        AddBuildingsToMap(map, map_item);
        AddOfficesToMap(map, map_item);
        
        game.AddMap(std::move(map));
    }
    
    return game;
}

}  // namespace json_loader