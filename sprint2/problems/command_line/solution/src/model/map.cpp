#include "map.h"
#include "model_key_storage.h"
#include "logger.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roadmap_.GetRoads();
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

void Map::AddRoad(const Road& road) {
    roadmap_.AddRoad(road);
}

void Map::AddRoads(const Roads& roads){
    for(const auto& road : roads){
        AddRoad(road);
    }
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddBuildings(const Buildings& buildings){
    for(const auto& building : buildings){
        AddBuilding(building);
    }
}

void Map::AddOffice(const Office& office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(office);
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Map::AddOffices(const Offices& offices){
    for(const auto& office : offices){
        AddOffice(office);
    }
}
    
void Map::SetDogVelocity(const double velocity) {
    dog_velocity_ = std::abs(velocity);
}

double Map::GetDogVelocity() const noexcept {
    return dog_velocity_;
}

std::tuple<Position, Velocity> Map::GetValidMove(const Position& old_position,
                                                const Position& potential_new_position,
                                                const Velocity& old_velocity) {
    return roadmap_.GetValidMove(old_position, potential_new_position, old_velocity);
}

void tag_invoke(json::value_from_tag, json::value& jv, const Map& map) {
    jv = {{MAP_ID, json::value_from(*(map.GetId()))},
            {MAP_NAME, json::value_from(map.GetName())},
            {ROADS, json::value_from(map.GetRoads())},
            {BUILDINGS, json::value_from(map.GetBuildings())},
            {OFFICES, json::value_from(map.GetOffices())}};
}

Map tag_invoke(json::value_to_tag<Map>, const json::value& jv) {
    try {
        if (!jv.is_object()) {
            throw std::runtime_error("Map JSON is not an object");
        }

        const auto& obj = jv.as_object();
        
        if (!obj.contains(MAP_ID)) {
            throw std::runtime_error("Missing required field: " + MAP_ID);
        }
        if (!obj.contains(MAP_NAME)) {
            throw std::runtime_error("Missing required field: " + MAP_NAME);
        }

        Map::Id id{json::value_to<std::string>(obj.at(MAP_ID))};
        std::string name = json::value_to<std::string>(obj.at(MAP_NAME));
        Map map(id, name);
        
        // Парсинг дорог (обязательно)
        try {
            if (!obj.contains(ROADS)) {
                throw std::runtime_error("Missing required field: " + ROADS + " for map " + *id);
            }
            
            const auto& roads_val = obj.at(ROADS);
            if (!roads_val.is_array()) {
                throw std::runtime_error(ROADS + " must be an array for map " + *id);
            }
            
            std::vector<Road> roads = json::value_to<std::vector<Road>>(roads_val);
            map.AddRoads(roads);
            
            BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                logware::ExceptionLogData(0, "Successfully parsed roads for map " + *id, 
                    "count: " + std::to_string(roads.size())));
                    
        } catch (const json::system_error& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "JSON conversion error for roads in map " + *id, e.what()));
            throw;
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Failed to parse roads for map " + *id, e.what()));
            throw;
        }
        
        // Парсинг зданий (опционально)
        try {
            if (obj.contains(BUILDINGS)) {
                const auto& buildings_val = obj.at(BUILDINGS);
                if (!buildings_val.is_array()) {
                    BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                        logware::ExceptionLogData(0, BUILDINGS + " must be an array for map " + *id, 
                            "ignoring buildings"));
                } else {
                    std::vector<Building> buildings = json::value_to<std::vector<Building>>(buildings_val);
                    map.AddBuildings(buildings);
                    
                    BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                        logware::ExceptionLogData(0, "Successfully parsed buildings for map " + *id, 
                            "count: " + std::to_string(buildings.size())));
                }
            }
        } catch (const json::system_error& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "JSON conversion error for buildings in map " + *id, e.what()));
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Failed to parse buildings for map " + *id, e.what()));
        }
        
        // Парсинг офисов (опционально)
        try {
            if (obj.contains(OFFICES)) {
                const auto& offices_val = obj.at(OFFICES);
                if (!offices_val.is_array()) {
                    BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                        logware::ExceptionLogData(0, OFFICES + " must be an array for map " + *id, 
                            "ignoring offices"));
                } else {
                    std::vector<Office> offices = json::value_to<std::vector<Office>>(offices_val);
                    map.AddOffices(offices);
                    
                    BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                        logware::ExceptionLogData(0, "Successfully parsed offices for map " + *id, 
                            "count: " + std::to_string(offices.size())));
                }
            }
        } catch (const json::system_error& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "JSON conversion error for offices in map " + *id, e.what()));
        } catch (const std::invalid_argument& e) {
            BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                logware::ExceptionLogData(0, "Duplicate office in map " + *id, e.what()));
            throw;
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Failed to parse offices for map " + *id, e.what()));
        }
        
        // Парсинг скорости собаки (опционально)
        try {
            if (obj.contains(MAP_DOG_VELOCITY)) {
                const auto& velocity_val = obj.at(MAP_DOG_VELOCITY);
                
                if (!velocity_val.is_double() && !velocity_val.is_int64()) {
                    BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                        logware::ExceptionLogData(0, "dogSpeed has invalid type for map " + *id, 
                            std::string(velocity_val.kind() == json::kind::double_ ? "double" : 
                                       velocity_val.kind() == json::kind::int64 ? "int64" : 
                                       velocity_val.kind() == json::kind::uint64 ? "uint64" : "other")));
                } else {
                    double dog_velocity = json::value_to<double>(velocity_val);
                    
                    if (dog_velocity <= 0) {
                        BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                            logware::ExceptionLogData(0, "dogSpeed is not positive for map " + *id, 
                                std::to_string(dog_velocity)));
                    }
                    
                    if (dog_velocity > 100) {
                        BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                            logware::ExceptionLogData(0, "dogSpeed is unusually high for map " + *id, 
                                std::to_string(dog_velocity)));
                    }
                    
                    map.SetDogVelocity(dog_velocity);
                    
                    BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                        logware::ExceptionLogData(0, "Set dog velocity for map " + *id, 
                            std::to_string(dog_velocity)));
                }
            }
        } catch (const json::system_error& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "JSON conversion error for dogSpeed in map " + *id, e.what()));
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Unexpected error parsing dogSpeed for map " + *id, e.what()));
        }
        
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("info"sv,
            logware::ExceptionLogData(0, "Successfully loaded map " + *id, name));
        
        return map;
        
    } catch (const json::system_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "JSON system error in map parsing", e.what()));
        throw;
    } catch (const std::out_of_range& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Missing required field in map JSON", e.what()));
        throw;
    } catch (const std::invalid_argument& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Invalid argument in map parsing", e.what()));
        throw;
    } catch (const std::runtime_error& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Runtime error in map parsing", e.what()));
        throw;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Unexpected error in map parsing", e.what()));
        throw;
    }
}

}