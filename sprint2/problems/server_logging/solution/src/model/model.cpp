#include "model.h"

#include <stdexcept>

#include "model_key_storage.h"

namespace model {

using namespace std::literals;

void Map::AddOffice(Office office) {
  if (warehouse_id_to_index_.contains(office.GetId())) {
    throw std::invalid_argument("Duplicate warehouse");
  }

  const size_t index = offices_.size();
  Office& office_ref = offices_.emplace_back(std::move(office));
  try {
    warehouse_id_to_index_.emplace(office_ref.GetId(), index);
  } catch (...) {
    offices_.pop_back();
    throw;
  }
}

void Game::AddMap(Map map) {
  const size_t index = maps_.size();
  auto [iterator, inserted] =
      map_id_to_index_.emplace(map.GetId(), index);
  if (!inserted) {
    throw std::invalid_argument("Map with id "s + *map.GetId() +
                                " already exists"s);
  }

  try {
    maps_.emplace_back(std::move(map));
  } catch (...) {
    map_id_to_index_.erase(iterator);
    throw;
  }
}

void Game::AddMaps(std::vector<Map>& maps) {
  for (const auto& map : maps) {
    AddMap(map);
  }
}

void tag_invoke(json::value_from_tag, json::value& jv, const Road& road) {
  if (road.IsHorizontal()) {
    jv = {{kRoadX0, json::value_from(road.GetStart().x)},
          {kRoadY0, json::value_from(road.GetStart().y)},
          {kRoadX1, json::value_from(road.GetEnd().x)}};
  } else {
    jv = {{kRoadX0, json::value_from(road.GetStart().x)},
          {kRoadY0, json::value_from(road.GetStart().y)},
          {kRoadY1, json::value_from(road.GetEnd().y)}};
  }
}

Road tag_invoke(json::value_to_tag<Road>, const json::value& jv) {
  Point start;
  start.x = json::value_to<int>(jv.as_object().at(kRoadX0));
  start.y = json::value_to<int>(jv.as_object().at(kRoadY0));
  Coord end;

  try {
    end = json::value_to<int>(jv.as_object().at(kRoadX1));
    return Road(Road::HORIZONTAL, start, end);
  } catch (...) {
    end = json::value_to<int>(jv.as_object().at(kRoadY1));
    return Road(Road::VERTICAL, start, end);
  }
}

void tag_invoke(json::value_from_tag, json::value& jv,
                const Building& building) {
  jv = {{kBuildingX, json::value_from(building.GetBounds().position.x)},
        {kBuildingY, json::value_from(building.GetBounds().position.y)},
        {kBuildingWidth, json::value_from(building.GetBounds().size.width)},
        {kBuildingHeight, json::value_from(building.GetBounds().size.height)}};
}

Building tag_invoke(json::value_to_tag<Building>, const json::value& jv) {
  Point point;
  point.x = json::value_to<int>(jv.as_object().at(kBuildingX));
  point.y = json::value_to<int>(jv.as_object().at(kBuildingY));
  Size size;
  size.width = json::value_to<int>(jv.as_object().at(kBuildingWidth));
  size.height = json::value_to<int>(jv.as_object().at(kBuildingHeight));
  return Building(Rectangle(point, size));
}

void tag_invoke(json::value_from_tag, json::value& jv, const Office& office) {
  jv = {{kOfficeId, json::value_from(*(office.GetId()))},
        {kOfficeX, json::value_from(office.GetPosition().x)},
        {kOfficeY, json::value_from(office.GetPosition().y)},
        {kOfficeOffsetX, json::value_from(office.GetOffset().dx)},
        {kOfficeOffsetY, json::value_from(office.GetOffset().dy)}};
}

Office tag_invoke(json::value_to_tag<Office>, const json::value& jv) {
  Office::Id id{json::value_to<std::string>(jv.as_object().at(kOfficeId))};
  Point position;
  position.x = json::value_to<int>(jv.as_object().at(kOfficeX));
  position.y = json::value_to<int>(jv.as_object().at(kOfficeY));
  Offset offset;
  offset.dx = json::value_to<int>(jv.as_object().at(kOfficeOffsetX));
  offset.dy = json::value_to<int>(jv.as_object().at(kOfficeOffsetY));
  return Office(id, position, offset);
}

void tag_invoke(json::value_from_tag, json::value& jv, const Map& map) {
  jv = {{kMapId, json::value_from(*(map.GetId()))},
        {kMapName, json::value_from(map.GetName())},
        {kRoads, json::value_from(map.GetRoads())},
        {kBuildings, json::value_from(map.GetBuildings())},
        {kOffices, json::value_from(map.GetOffices())}};
}

Map tag_invoke(json::value_to_tag<Map>, const json::value& jv) {
  Map::Id id{json::value_to<std::string>(jv.as_object().at(kMapId))};
  std::string name = json::value_to<std::string>(jv.as_object().at(kMapName));
  Map map(id, name);

  std::vector<Road> roads =
      json::value_to<std::vector<Road>>(jv.as_object().at(kRoads));
  map.AddRoads(roads);

  std::vector<Building> buildings =
      json::value_to<std::vector<Building>>(jv.as_object().at(kBuildings));
  map.AddBuildings(buildings);

  std::vector<Office> offices =
      json::value_to<std::vector<Office>>(jv.as_object().at(kOffices));
  map.AddOffices(offices);

  return map;
}

}