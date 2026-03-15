#include "json_model_converter.h"
#include "model_key_storage.h"
#include "logger.h"

namespace model {

LootGeneratorConfig tag_invoke(json::value_to_tag<LootGeneratorConfig>, const json::value& jv) {
    try {
        LootGeneratorConfig config;
        config.period = json::value_to<double>(jv.as_object().at(LOOT_GENERATOR_PERIOD));
        config.probability = json::value_to<double>(jv.as_object().at(LOOT_GENERATOR_PROBABILITY));
        return config;
    } catch(const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << "Missing loot generator config field: " << e.what();
        throw;
    } catch(const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << "JSON parsing error in loot generator config: " << e.what();
        throw;
    }
};


void tag_invoke(json::value_from_tag, json::value& jv, const LootType& loot_type) {
    json::object res = {
        {LOOT_TYPES_NAME, json::value_from(loot_type.name)},
        {LOOT_TYPES_FILE, json::value_from(loot_type.file)},
        {LOOT_TYPES_TYPE, json::value_from(loot_type.type)},
        {LOOT_TYPES_SCALE, json::value_from(loot_type.scale)},
        {LOOT_TYPES_VALUE, json::value_from(loot_type.value)}
    };
    if(loot_type.color) {
        res[LOOT_TYPES_COLOR] = json::value_from(loot_type.color.value());
    }
    if(loot_type.rotation) {
        res[LOOT_TYPES_ROTATION] = json::value_from(loot_type.rotation.value());
    }
    jv.emplace_object() = res;
};

LootType tag_invoke(json::value_to_tag<LootType>, const json::value& jv) {
    try {
        LootType loot_type;
        loot_type.name = json::value_to<std::string>(jv.as_object().at(LOOT_TYPES_NAME));
        loot_type.file = json::value_to<std::string>(jv.as_object().at(LOOT_TYPES_FILE));
        loot_type.type = json::value_to<std::string>(jv.as_object().at(LOOT_TYPES_TYPE));
        loot_type.value = json::value_to<size_t>(jv.as_object().at(LOOT_TYPES_VALUE));
        
        try {
            loot_type.rotation = json::value_to<int>(jv.as_object().at(LOOT_TYPES_ROTATION));
        } catch (const std::out_of_range& e) {
           
        }
        
        try {
            loot_type.color = json::value_to<std::string>(jv.as_object().at(LOOT_TYPES_COLOR));
        } catch (const std::out_of_range& e) {
           
        }
        
        loot_type.scale = json::value_to<double>(jv.as_object().at(LOOT_TYPES_SCALE));
        return loot_type;
    } catch(const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << "Missing loot type field: " << e.what();
        throw;
    } catch(const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << "JSON parsing error in loot type: " << e.what();
        throw;
    }
};

void tag_invoke(json::value_from_tag, json::value& jv, const Building& building) {
    jv = {{BUILDING_X, json::value_from(building.GetBounds().position.x)},
            {BUILDING_Y, json::value_from(building.GetBounds().position.y)},
            {BUILDING_WIDTH, json::value_from(building.GetBounds().size.width)},
            {BUILDING_HEIGHT, json::value_from(building.GetBounds().size.height)}};
};

Building tag_invoke(json::value_to_tag<Building>, const json::value& jv) {
    try {
        Point point;
        point.x = json::value_to<int>(jv.as_object().at(BUILDING_X));
        point.y = json::value_to<int>(jv.as_object().at(BUILDING_Y));
        Size size;
        size.width = json::value_to<int>(jv.as_object().at(BUILDING_WIDTH));
        size.height = json::value_to<int>(jv.as_object().at(BUILDING_HEIGHT));
        return Building(Rectangle(point, size));
    } catch(const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << "Missing building field: " << e.what();
        throw;
    } catch(const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << "JSON parsing error in building: " << e.what();
        throw;
    }
};

void tag_invoke(json::value_from_tag, json::value& jv, const Office& office) {
    jv = {{OFFICE_ID, json::value_from(*(office.GetId()))},
            {OFFICE_X, json::value_from(office.GetPosition().x)},
            {OFFICE_Y, json::value_from(office.GetPosition().y)},
            {OFFICE_OFFSET_X, json::value_from(office.GetOffset().dx)},
            {OFFICE_OFFSET_Y, json::value_from(office.GetOffset().dy)}};
};

Office tag_invoke(json::value_to_tag<Office>, const json::value& jv) {
    try {
        Office::Id id{json::value_to<std::string>(jv.as_object().at(OFFICE_ID))};
        geom::Point2D position;
        position.x = json::value_to<int>(jv.as_object().at(OFFICE_X));
        position.y = json::value_to<int>(jv.as_object().at(OFFICE_Y));
        Offset offset;
        offset.dx = json::value_to<int>(jv.as_object().at(OFFICE_OFFSET_X));
        offset.dy = json::value_to<int>(jv.as_object().at(OFFICE_OFFSET_Y));
        return Office(id, position, offset);
    } catch(const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << "Missing office field: " << e.what();
        throw;
    } catch(const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << "JSON parsing error in office: " << e.what();
        throw;
    }
};


void tag_invoke(json::value_from_tag, json::value& jv, const Road& road) {
    if(road.IsHorizontal()) {
        jv = {{ROAD_XO, json::value_from(road.GetStart().x)},
                {ROAD_YO, json::value_from(road.GetStart().y)},
                {ROAD_X1, json::value_from(road.GetEnd().x)}};
    } else {
        jv = {{ROAD_XO, json::value_from(road.GetStart().x)},
                {ROAD_YO, json::value_from(road.GetStart().y)},
                {ROAD_Y1, json::value_from(road.GetEnd().y)}};
    }
};

Road tag_invoke(json::value_to_tag<Road>, const json::value& jv) {
    try {
        Point start;
        start.x = json::value_to<int>(jv.as_object().at(ROAD_XO));
        start.y = json::value_to<int>(jv.as_object().at(ROAD_YO));
        Coord end;
        try {
            end = json::value_to<int>(jv.as_object().at(ROAD_X1));
            return Road(Road::HORIZONTAL, start, end);
        } catch (...) {
            end = json::value_to<int>(jv.as_object().at(ROAD_Y1));
            return Road(Road::VERTICAL, start, end);
        }
    } catch(const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << "Missing road field: " << e.what();
        throw;
    } catch(const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << "JSON parsing error in road: " << e.what();
        throw;
    }
};


void tag_invoke(json::value_from_tag, json::value& jv, const Map& map) {
    jv = {{MAP_ID, json::value_from(*(map.GetId()))},
            {MAP_NAME, json::value_from(map.GetName())},
            {ROADS, json::value_from(map.GetRoads())},
            {BUILDINGS, json::value_from(map.GetBuildings())},
            {OFFICES, json::value_from(map.GetOffices())},
            {LOOT_TYPES, json::value_from(map.GetLootTypes())}
        };
};

Map tag_invoke(json::value_to_tag<Map>, const json::value& jv) {
    try {
        Map::Id id{json::value_to<std::string>(jv.as_object().at(MAP_ID))};
        std::string name = json::value_to<std::string>(jv.as_object().at(MAP_NAME));
        Map map(id, name);
        
        std::vector<Road> roads = json::value_to< std::vector<Road> >(jv.as_object().at(ROADS));
        map.AddRoads(roads);
        
        std::vector<Building> buildings = json::value_to<std::vector<Building>>(jv.as_object().at(BUILDINGS));
        map.AddBuildings(buildings);
        
        std::vector<Office> offices = json::value_to<std::vector<Office>>(jv.as_object().at(OFFICES));
        map.AddOffices(offices);
        
        std::vector<LootType> look_types = json::value_to<std::vector<LootType>>(jv.as_object().at(LOOT_TYPES));
        if(look_types.empty()) {
            throw EmptyLootTypesOnMapException(*id);
        }
        map.AddLootTypes(look_types);
        
        try {
            double dog_velocity = json::value_to<double>(jv.as_object().at(MAP_DOG_VELOCITY));
            map.SetDogVelocity(dog_velocity);
        } catch(const std::out_of_range& e) {
           
        }
        
        try {
            double bag_capacity = json::value_to<double>(jv.as_object().at(MAP_BAG_CAPACITY));
            map.SetBagCapacity(bag_capacity);
        } catch(const std::out_of_range& e) {
           
        }
        
        return map;
    } catch(const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << "Missing map field: " << e.what();
        throw;
    } catch(const boost::json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << "JSON parsing error in map: " << e.what();
        throw;
    }
};

}
