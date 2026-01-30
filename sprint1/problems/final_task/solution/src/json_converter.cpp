#include "json_converter.h"
#include <sstream>

namespace json_converter {

namespace constants {
    // Common fields
    constexpr const char* ID = "id";
    constexpr const char* NAME = "name";
    constexpr const char* X = "x";
    constexpr const char* Y = "y";
    constexpr const char* OFFSET_X = "offsetX";
    constexpr const char* OFFSET_Y = "offsetY";
    
    // Map related
    constexpr const char* MAPS = "maps";
    constexpr const char* ROADS = "roads";
    constexpr const char* BUILDINGS = "buildings";
    constexpr const char* OFFICES = "offices";
    constexpr const char* WIDTH = "width";
    constexpr const char* HEIGHT = "height";
    
    // Error related
    constexpr const char* ERROR_CODE = "code";
    constexpr const char* ERROR_MESSAGE = "message";
    
    // Road types
    constexpr const char* ROAD_START = "x0";
    constexpr const char* ROAD_END = "x1";
    constexpr const char* ROAD_START_VERTICAL = "y0";
    constexpr const char* ROAD_END_VERTICAL = "y1";
}

namespace {

std::string ConvertToJsonString(const Json::Value& json_value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, json_value);
}

Json::Value CreateRoadJson(const model::Road& road) {
    Json::Value json_road;
    
    if (road.IsHorizontal()) {
        json_road[constants::ROAD_START] = road.GetStart().x;
        json_road[constants::ROAD_END] = road.GetEnd().x;
        json_road[constants::Y] = road.GetStart().y;
    } else {  // Vertical road
        json_road[constants::X] = road.GetStart().x;
        json_road[constants::ROAD_START_VERTICAL] = road.GetStart().y;
        json_road[constants::ROAD_END_VERTICAL] = road.GetEnd().y;
    }
    
    return json_road;
}

Json::Value CreateBuildingJson(const model::Building& building) {
    Json::Value json_building;
    
    json_building[constants::X] = building.GetBounds().position.x;
    json_building[constants::Y] = building.GetBounds().position.y;
    json_building[constants::WIDTH] = building.GetBounds().size.width;
    json_building[constants::HEIGHT] = building.GetBounds().size.height;
    
    return json_building;
}

Json::Value CreateOfficeJson(const model::Office& office) {
    Json::Value json_office;
    
    json_office[constants::ID] = *office.GetId();
    json_office[constants::X] = office.GetPosition().x;
    json_office[constants::Y] = office.GetPosition().y;
    json_office[constants::OFFSET_X] = office.GetOffset().dx;
    json_office[constants::OFFSET_Y] = office.GetOffset().dy;
    
    return json_office;
}

Json::Value CreateErrorResponse(const std::string& code, const std::string& message) {
    Json::Value json_error;
    json_error[constants::ERROR_CODE] = code;
    json_error[constants::ERROR_MESSAGE] = message;
    return json_error;
}

}  // namespace

std::string ConvertMapListToJson(const model::Game& game) {
    Json::Value json_maps;
    
    for (const auto& map : game.GetMaps()) {
        Json::Value json_map;
        json_map[constants::ID] = *map.GetId();
        json_map[constants::NAME] = map.GetName();
        json_maps.append(json_map);
    }
    
    return ConvertToJsonString(json_maps);
}

std::string ConvertMapToJson(const model::Map& map) {
    Json::Value json_map;
    
    // Basic info
    json_map[constants::ID] = *map.GetId();
    json_map[constants::NAME] = map.GetName();
    
    // Roads
    Json::Value json_roads;
    for (const auto& road : map.GetRoads()) {
        json_roads.append(CreateRoadJson(road));
    }
    json_map[constants::ROADS] = json_roads;
    
    // Buildings
    Json::Value json_buildings;
    for (const auto& building : map.GetBuildings()) {
        json_buildings.append(CreateBuildingJson(building));
    }
    json_map[constants::BUILDINGS] = json_buildings;
    
    // Offices
    Json::Value json_offices;
    for (const auto& office : map.GetOffices()) {
        json_offices.append(CreateOfficeJson(office));
    }
    json_map[constants::OFFICES] = json_offices;
    
    return ConvertToJsonString(json_map);
}

std::string CreateMapNotFoundResponse() {
    Json::Value json_error = CreateErrorResponse("mapNotFound", "Map not found");
    return ConvertToJsonString(json_error);
}

std::string CreateBadRequestResponse() {
    Json::Value json_error = CreateErrorResponse("badRequest", "Bad request");
    return ConvertToJsonString(json_error);
}

std::string CreatePageNotFoundResponse() {
    Json::Value json_error = CreateErrorResponse("pageNotFound", "Page not found");
    return ConvertToJsonString(json_error);
}

}  // namespace json_converter