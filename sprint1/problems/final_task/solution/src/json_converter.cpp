#include "json_converter.h"

#include <json/json.h>
#include <sstream>

namespace jsonConverter {

namespace {

Json::Value RoadToJson(const model::Road& road) {
    Json::Value json_road;
    json_road["x0"] = road.GetStart().x;
    json_road["y0"] = road.GetStart().y;
    
    if (road.IsHorizontal()) {
        json_road["x1"] = road.GetEnd().x;
    } else {
        json_road["y1"] = road.GetEnd().y;
    }
    
    return json_road;
}

Json::Value BuildingToJson(const model::Building& building) {
    Json::Value json_building;
    const auto& bounds = building.GetBounds();
    
    json_building["x"] = bounds.position.x;
    json_building["y"] = bounds.position.y;
    json_building["w"] = bounds.size.width;
    json_building["h"] = bounds.size.height;
    
    return json_building;
}

Json::Value OfficeToJson(const model::Office& office) {
    Json::Value json_office;
    
    json_office["id"] = (*office.GetId()).c_str();
    json_office["x"] = office.GetPosition().x;
    json_office["y"] = office.GetPosition().y;
    json_office["offsetX"] = office.GetOffset().dx;
    json_office["offsetY"] = office.GetOffset().dy;
    
    return json_office;
}

std::string CreateErrorResponse(const std::string& code, const std::string& message) {
    Json::Value root;
    root["code"] = code.c_str();
    root["message"] = message.c_str();
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::ostringstream stream;
    Json::StreamWriter* writer = builder.newStreamWriter();
    writer->write(root, &stream);
    delete writer;
    
    return stream.str();
}

}  // namespace

std::string ConvertMapListToJson(const model::Game& game) {
    Json::Value root(Json::arrayValue);
    
    for (const auto& map : game.GetMaps()) {
        Json::Value json_map;
        json_map["id"] = (*map.GetId()).c_str();
        json_map["name"] = map.GetName().c_str();
        root.append(json_map);
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::ostringstream stream;
    Json::StreamWriter* writer = builder.newStreamWriter();
    writer->write(root, &stream);
    delete writer;
    
    return stream.str();
}

std::string ConvertMapToJson(const model::Map& map) {
    Json::Value root;
    
    root["id"] = (*map.GetId()).c_str();
    root["name"] = map.GetName().c_str();
    
    Json::Value roads(Json::arrayValue);
    for (const auto& road : map.GetRoads()) {
        roads.append(RoadToJson(road));
    }
    root["roads"] = roads;
    
    Json::Value buildings(Json::arrayValue);
    for (const auto& building : map.GetBuildings()) {
        buildings.append(BuildingToJson(building));
    }
    root["buildings"] = buildings;
    
    Json::Value offices(Json::arrayValue);
    for (const auto& office : map.GetOffices()) {
        offices.append(OfficeToJson(office));
    }
    root["offices"] = offices;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::ostringstream stream;
    Json::StreamWriter* writer = builder.newStreamWriter();
    writer->write(root, &stream);
    delete writer;
    
    return stream.str();
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