#include "json_loader.h"
#include "json_key_storage.h"

#include <fstream>
#include <json/json.h>
#include <iostream>

namespace json_loader {

using namespace Json;

Value ReadFile(const std::filesystem::path& json_path) {
  std::ifstream file(json_path);
  if (!file.is_open()) {
    throw std::runtime_error("Can't open file: " + json_path.string());
  }
  
  Value root;
  file >> root;
  return root;
}

void CheckElementExisting(Value& node, const std::string& name) {
  if (!node.find(&name[0], &name[name.size()])) {
    throw std::runtime_error("Element not found: " + name);
  }
}

bool HasElement(Value& node, const std::string& name) {
  return node.find(&name[0], &name[name.size()]) != nullptr;
}

void AddRoadsToMap(model::Map& map, Value& map_item) {
  CheckElementExisting(map_item, json_keys::kRoads);
  
  for (auto& item : map_item[json_keys::kRoads]) {
    CheckElementExisting(item, json_keys::kRoadX0);
    model::Coord x{item[json_keys::kRoadX0].asInt()};

    CheckElementExisting(item, json_keys::kRoadY0);
    model::Coord y{item[json_keys::kRoadY0].asInt()};

    model::Point start_point(x, y);
    
    if (HasElement(item, json_keys::kRoadX1)) {
      model::Coord end{item[json_keys::kRoadX1].asInt()};
      map.AddRoad(model::Road(model::Road::HORIZONTAL, start_point, end));  
    } else {
      CheckElementExisting(item, json_keys::kRoadY1);
      model::Coord end{item[json_keys::kRoadY1].asInt()};
      map.AddRoad(model::Road(model::Road::VERTICAL, start_point, end));
    }
  }
}

void AddBuildingsToMap(model::Map& map, Value& map_item) {
  CheckElementExisting(map_item, json_keys::kBuildings);
  
  for (auto& item : map_item[json_keys::kBuildings]) {
    CheckElementExisting(item, json_keys::kBuildingX);
    model::Coord x{item[json_keys::kBuildingX].asInt()};

    CheckElementExisting(item, json_keys::kBuildingY);
    model::Coord y{item[json_keys::kBuildingY].asInt()};

    CheckElementExisting(item, json_keys::kBuildingWidth);
    model::Coord w{item[json_keys::kBuildingWidth].asInt()};

    CheckElementExisting(item, json_keys::kBuildingHeight);
    model::Coord h{item[json_keys::kBuildingHeight].asInt()};

    model::Rectangle rect{model::Point{x, y}, model::Size{w, h}};
    map.AddBuilding(model::Building(std::move(rect)));
  }
}

void AddOfficesToMap(model::Map& map, Value& map_item) {
  CheckElementExisting(map_item, json_keys::kOffices);
  
  for (auto& item : map_item[json_keys::kOffices]) {
    CheckElementExisting(item, json_keys::kOfficeId);
    model::Office::Id id_office(item[json_keys::kOfficeId].asCString());

    CheckElementExisting(item, json_keys::kOfficeX);
    model::Coord x{item[json_keys::kOfficeX].asInt()};

    CheckElementExisting(item, json_keys::kOfficeY);
    model::Coord y{item[json_keys::kOfficeY].asInt()};

    CheckElementExisting(item, json_keys::kOfficeOffsetX);
    model::Coord dx{item[json_keys::kOfficeOffsetX].asInt()};

    CheckElementExisting(item, json_keys::kOfficeOffsetY);
    model::Coord dy{item[json_keys::kOfficeOffsetY].asInt()};

    map.AddOffice(model::Office(id_office,
                                model::Point(x, y),
                                model::Offset(dx, dy)));
  }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
  Value map_json = ReadFile(json_path);
  model::Game game;

  CheckElementExisting(map_json, json_keys::kMaps);
  
  for (auto& map_item : map_json[json_keys::kMaps]) {
    CheckElementExisting(map_item, json_keys::kMapId);
    std::string id(map_item[json_keys::kMapId].asCString());

    CheckElementExisting(map_item, json_keys::kMapName);
    model::Map map(model::Map::Id(id), map_item[json_keys::kMapName].asCString());

    AddRoadsToMap(map, map_item);
    AddBuildingsToMap(map, map_item);
    AddOfficesToMap(map, map_item);

    game.AddMap(std::move(map));
  }
  
  return game;
}

}  // namespace json_loader