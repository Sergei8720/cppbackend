#include "json_converter.h"

#include <json/json.h>
#include <sstream>

namespace jsonConverter {

namespace {

const char* const kIdKey = "id";
const char* const kXKey = "x";
const char* const kYKey = "y";
const char* const kWKey = "w";
const char* const kHKey = "h";
const char* const kX0Key = "x0";
const char* const kY0Key = "y0";
const char* const kX1Key = "x1";
const char* const kY1Key = "y1";
const char* const kOffsetXKey = "offsetX";
const char* const kOffsetYKey = "offsetY";
const char* const kNameKey = "name";
const char* const kRoadsKey = "roads";
const char* const kBuildingsKey = "buildings";
const char* const kOfficesKey = "offices";
const char* const kCodeKey = "code";
const char* const kMessageKey = "message";

Json::Value RoadToJson(const model::Road& road) {
    Json::Value json_road;
    json_road[kX0Key] = road.GetStart().x;
    json_road[kY0Key] = road.GetStart().y;
    
    if (road.IsHorizontal()) {
        json_road[kX1Key] = road.GetEnd().x;
    } else {
        json_road[kY1Key] = road.GetEnd().y;
    }
    
    return json_road;
}

Json::Value BuildingToJson(const model::Building& building) {
    Json::Value json_building;
    const auto& bounds = building.GetBounds();
    
    json_building[kXKey] = bounds.position.x;
    json_building[kYKey] = bounds.position.y;
    json_building[kWKey] = bounds.size.width;
    json_building[kHKey] = bounds.size.height;
    
    return json_building;
}

Json::Value OfficeToJson(const model::Office& office) {
    Json::Value json_office;
    
    json_office[kIdKey] = *office.GetId();
    json_office[kXKey] = office.GetPosition().x;
    json_office[kYKey] = office.GetPosition().y;
    json_office[kOffsetXKey] = office.GetOffset().dx;
    json_office[kOffsetYKey] = office.GetOffset().dy;
    
    return json_office;
}

std::string CreateErrorResponse(const std::string& code, const std::string& message) {
    Json::Value root;
    root[kCodeKey] = code;
    root[kMessageKey] = message;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

}  // namespace

std::string ConvertMapListToJson(const model::Game& game) {
    Json::Value root(Json::arrayValue);
    
    for (const auto& map : game.GetMaps()) {
        Json::Value json_map;
        json_map[kIdKey] = *map.GetId();
        json_map[kNameKey] = map.GetName();
        root.append(json_map);
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

std::string ConvertMapToJson(const model::Map& map) {
    Json::Value root;
    
    root[kIdKey] = *map.GetId();
    root[kNameKey] = map.GetName();
    
    Json::Value roads(Json::arrayValue);
    for (const auto& road : map.GetRoads()) {
        roads.append(RoadToJson(road));
    }
    root[kRoadsKey] = roads;
    
    Json::Value buildings(Json::arrayValue);
    for (const auto& building : map.GetBuildings()) {
        buildings.append(BuildingToJson(building));
    }
    root[kBuildingsKey] = buildings;
    
    Json::Value offices(Json::arrayValue);
    for (const auto& office : map.GetOffices()) {
        offices.append(OfficeToJson(office));
    }
    root[kOfficesKey] = offices;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
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