#include "json_converter.h"
#include <sstream>

namespace json_converter {
namespace json = boost::json;

std::string ConvertMapsToJson(const std::vector<model::Map>& maps) {
    json::array arr;
    for (const auto& map : maps) {
        json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        arr.push_back(obj);
    }
    return json::serialize(arr);
}

std::string ConvertMapToJson(const model::Map& map) {
    json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    
    // Добавляем дороги
    json::array roads;
    for (const auto& road : map.GetRoads()) {
        json::object road_obj;
        if (road.IsHorizontal()) {
            road_obj["x0"] = road.GetStart().x;
            road_obj["y0"] = road.GetStart().y;
            road_obj["x1"] = road.GetEnd().x;
        } else {
            road_obj["x0"] = road.GetStart().x;
            road_obj["y0"] = road.GetStart().y;
            road_obj["y1"] = road.GetEnd().y;
        }
        roads.push_back(road_obj);
    }
    obj["roads"] = roads;
    
    // Добавляем здания
    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        json::object building_obj;
        building_obj["x"] = building.GetBounds().position.x;
        building_obj["y"] = building.GetBounds().position.y;
        building_obj["w"] = building.GetBounds().size.width;
        building_obj["h"] = building.GetBounds().size.height;
        buildings.push_back(building_obj);
    }
    obj["buildings"] = buildings;
    
    // Добавляем офисы
    json::array offices;
    for (const auto& office : map.GetOffices()) {
        json::object office_obj;
        office_obj["id"] = *office.GetId();
        office_obj["x"] = office.GetPosition().x;
        office_obj["y"] = office.GetPosition().y;
        office_obj["offsetX"] = office.GetOffset().dx;
        office_obj["offsetY"] = office.GetOffset().dy;
        offices.push_back(office_obj);
    }
    obj["offices"] = offices;
    
    return json::serialize(obj);
}

} // namespace json_converter