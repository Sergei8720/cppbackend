#include "json_converter.h"
#include "json_key_storage.h"

#include <json/json.h>
#include <map>
#include <sstream>

namespace json_converter {

std::string ConvertMapListToJson(const model::Game& game) {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  
  for (const auto& item : game.GetMaps()) {
    Json::Value map;
    map[json_keys::kMapId] = (*item.GetId()).c_str();
    map[json_keys::kMapName] = item.GetName().c_str();
    root.append(std::move(map));
  }
  
  std::stringstream json_string;
  writer->write(root, &json_string);
  return json_string.str();
}

void AddRoadsToJson(const model::Map& map, Json::Value& root) {
  Json::Value roads;
  
  for (const auto& item : map.GetRoads()) {
    Json::Value road;
    road[json_keys::kRoadX0] = item.GetStart().x;
    road[json_keys::kRoadY0] = item.GetStart().y;
    
    if (item.IsVertical()) {
      road[json_keys::kRoadY1] = item.GetEnd().y;
    } else {
      road[json_keys::kRoadX1] = item.GetEnd().x;
    }
    
    roads.append(std::move(road));
  }
  
  root[json_keys::kRoads] = roads;
}

void AddBuildingsToJson(const model::Map& map, Json::Value& root) {
  Json::Value buildings;
  
  for (const auto& item : map.GetBuildings()) {
    Json::Value building;
    building[json_keys::kBuildingX] = item.GetBounds().position.x;
    building[json_keys::kBuildingY] = item.GetBounds().position.y;
    building[json_keys::kBuildingWidth] = item.GetBounds().size.width;
    building[json_keys::kBuildingHeight] = item.GetBounds().size.height;
    buildings.append(std::move(building));
  }
  
  root[json_keys::kBuildings] = buildings;
}

void AddOfficesToJson(const model::Map& map, Json::Value& root) {
  Json::Value offices;
  
  for (const auto& item : map.GetOffices()) {
    Json::Value office;
    office[json_keys::kOfficeId] = (*item.GetId()).c_str();
    office[json_keys::kOfficeX] = item.GetPosition().x;
    office[json_keys::kOfficeY] = item.GetPosition().y;
    office[json_keys::kOfficeOffsetX] = item.GetOffset().dx;
    office[json_keys::kOfficeOffsetY] = item.GetOffset().dy;
    offices.append(std::move(office));
  }
  
  root[json_keys::kOffices] = offices;
}

std::string ConvertMapToJson(const model::Map& map) {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  
  root[json_keys::kMapId] = (*map.GetId()).c_str();
  root[json_keys::kMapName] = map.GetName().c_str();
  
  AddRoadsToJson(map, root);
  AddBuildingsToJson(map, root);
  AddOfficesToJson(map, root);
  
  std::stringstream json_string;
  writer->write(root, &json_string);
  return json_string.str();
}

std::string CreateMapNotFoundResponse() {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  
  root[json_keys::kResponseCode] = "mapNotFound";
  root[json_keys::kResponseMessage] = "Map not found";
  
  std::stringstream json_string;
  writer->write(root, &json_string);
  return json_string.str();
}

std::string CreateBadRequestResponse() {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  
  root[json_keys::kResponseCode] = "badRequest";
  root[json_keys::kResponseMessage] = "Bad request";
  
  std::stringstream json_string;
  writer->write(root, &json_string);
  return json_string.str();
}

std::string CreatePageNotFoundResponse() {
  Json::Value root;
  Json::StreamWriterBuilder builder;
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  
  root[json_keys::kResponseCode] = "pageNotFound";
  root[json_keys::kResponseMessage] = "Page not found";
  
  std::stringstream json_string;
  writer->write(root, &json_string);
  return json_string.str();
}

}  // namespace json_converter