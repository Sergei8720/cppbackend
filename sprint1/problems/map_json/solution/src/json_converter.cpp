#include "json_converter.h"

#include <sstream>
#include <boost/json.hpp>

namespace json_converter {

namespace json = boost::json;

namespace {

std::string JsonToString(const json::value& json_value) {
    std::ostringstream oss;
    oss << json_value;
    return oss.str();
}

void AddRoadToJson(const model::Road& road, json::array& roads_array) {
    json::object road_obj;
    road_obj["x0"] = road.GetStart().x;
    road_obj["y0"] = road.GetStart().y;
    
    if (road.IsHorizontal()) {
        road_obj["x1"] = road.GetEnd().x;
    } else {
        road_obj["y1"] = road.GetEnd().y;
    }
    
    roads_array.emplace_back(std::move(road_obj));
}

void AddBuildingToJson(const model::Building& building, json::array& buildings_array) {
    json::object building_obj;
    const auto& bounds = building.GetBounds();
    building_obj["x"] = bounds.position.x;
    building_obj["y"] = bounds.position.y;
    building_obj["w"] = bounds.size.width;
    building_obj["h"] = bounds.size.height;
    buildings_array.emplace_back(std::move(building_obj));
}

void AddOfficeToJson(const model::Office& office, json::array& offices_array) {
    json::object office_obj;
    office_obj["id"] = *office.GetId();
    office_obj["x"] = office.GetPosition().x;
    office_obj["y"] = office.GetPosition().y;
    office_obj["offsetX"] = office.GetOffset().dx;
    office_obj["offsetY"] = office.GetOffset().dy;
    offices_array.emplace_back(std::move(office_obj));
}

}  // namespace

std::string ConvertMapListToJson(const model::Game& game) {
    json::array maps_array;
    
    for (const auto& map : game.GetMaps()) {
        json::object map_obj;
        map_obj["id"] = *map.GetId();
        map_obj["name"] = map.GetName();
        maps_array.emplace_back(std::move(map_obj));
    }
    
    return JsonToString(json::value(std::move(maps_array)));
}

std::string ConvertMapToJson(const model::Map& map) {
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    
    json::array roads_array;
    for (const auto& road : map.GetRoads()) {
        AddRoadToJson(road, roads_array);
    }
    map_obj["roads"] = std::move(roads_array);
    
    json::array buildings_array;
    for (const auto& building : map.GetBuildings()) {
        AddBuildingToJson(building, buildings_array);
    }
    map_obj["buildings"] = std::move(buildings_array);
    
    json::array offices_array;
    for (const auto& office : map.GetOffices()) {
        AddOfficeToJson(office, offices_array);
    }
    map_obj["offices"] = std::move(offices_array);
    
    return JsonToString(json::value(std::move(map_obj)));
}

std::string CreateErrorResponse(std::string_view code, std::string_view message) {
    json::object error_obj;
    error_obj["code"] = code;
    error_obj["message"] = message;
    return JsonToString(json::value(std::move(error_obj)));
}

std::string CreateMapNotFoundResponse() {
    return CreateErrorResponse("mapNotFound", "Map not found");
}

std::string CreateBadRequestResponse() {
    return CreateErrorResponse("badRequest", "Bad request");
}

std::string CreatePageNotFoundResponse() {
    return CreateErrorResponse("pageNotFound", "Page not found");
}

}  // namespace json_converter