#include "json_converter.h"

#include <boost/json.hpp>
#include <boost/json/src.hpp>  // или подключите boost_json.cpp
#include <sstream>

namespace jsonConverter {

namespace json = boost::json;

namespace {

json::object RoadToJson(const model::Road& road) {
    json::object json_road;
    json_road["x0"] = road.GetStart().x;
    json_road["y0"] = road.GetStart().y;
    
    if (road.IsHorizontal()) {
        json_road["x1"] = road.GetEnd().x;
    } else {
        json_road["y1"] = road.GetEnd().y;
    }
    
    return json_road;
}

json::object BuildingToJson(const model::Building& building) {
    json::object json_building;
    const auto& bounds = building.GetBounds();
    
    json_building["x"] = bounds.position.x;
    json_building["y"] = bounds.position.y;
    json_building["w"] = bounds.size.width;
    json_building["h"] = bounds.size.height;
    
    return json_building;
}

json::object OfficeToJson(const model::Office& office) {
    json::object json_office;
    
    json_office["id"] = *office.GetId();
    json_office["x"] = office.GetPosition().x;
    json_office["y"] = office.GetPosition().y;
    json_office["offsetX"] = office.GetOffset().dx;
    json_office["offsetY"] = office.GetOffset().dy;
    
    return json_office;
}

std::string CreateErrorResponse(const std::string& code, const std::string& message) {
    json::object root;
    root["code"] = code;
    root["message"] = message;
    
    return json::serialize(root);
}

}  // namespace

std::string ConvertMapListToJson(const model::Game& game) {
    json::array root;
    
    for (const auto& map : game.GetMaps()) {
        json::object json_map;
        json_map["id"] = *map.GetId();
        json_map["name"] = map.GetName();
        root.push_back(json_map);
    }
    
    return json::serialize(root);
}

std::string ConvertMapToJson(const model::Map& map) {
    json::object root;
    
    root["id"] = *map.GetId();
    root["name"] = map.GetName();
    
    json::array roads;
    for (const auto& road : map.GetRoads()) {
        roads.push_back(RoadToJson(road));
    }
    root["roads"] = roads;
    
    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        buildings.push_back(BuildingToJson(building));
    }
    root["buildings"] = buildings;
    
    json::array offices;
    for (const auto& office : map.GetOffices()) {
        offices.push_back(OfficeToJson(office));
    }
    root["offices"] = offices;
    
    return json::serialize(root);
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

}  // namespace jsonConverter