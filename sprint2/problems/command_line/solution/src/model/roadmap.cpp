#include "roadmap.h"
#include "logger.h"

#include <cmath>
#include <iostream>
#include <stdint.h>
#include <set>
#include <algorithm>
#include <limits>

namespace model {

using namespace std::literals;

const int SCALE_FACTOR_OF_CELL = 20;

Roadmap::Roadmap(const Roadmap& other) {
    CopyContent(other.roads_);
}

Roadmap::Roadmap(Roadmap&& other) noexcept {
    matrix_map_ = std::move(other.matrix_map_);
    roads_ = std::move(other.roads_);
}

Roadmap& Roadmap::operator = (const Roadmap& other) {
    if(this != &other) {
        CopyContent(other.roads_);
    }
    return *this;
}

Roadmap& Roadmap::operator = (Roadmap&& other) noexcept {
    if(this != &other) {
        matrix_map_ = std::move(other.matrix_map_);
        roads_ = std::move(other.roads_);
    }
    return *this;
}

void Roadmap::FillMatrixForRoad(const Road& road, size_t index, int64_t start, int64_t end, 
                               int64_t fixed_coord, bool is_horizontal) {
    const int64_t SCALLED_OFFSET = static_cast<int64_t>(OFFSET * SCALE_FACTOR_OF_CELL);
    
    try {
        for (int64_t coord = start; coord <= end; ++coord) {
            for (int64_t i = -SCALLED_OFFSET; i <= SCALLED_OFFSET; ++i) {
                if (is_horizontal) {
                    // Для горизонтальной дороги: coord = x, фиксированная координата = y
                    matrix_map_[coord][fixed_coord + i].insert(index);
                } else {
                    // Для вертикальной дороги: coord = y, фиксированная координата = x
                    matrix_map_[fixed_coord + i][coord].insert(index);
                }
            }
        }
        
        BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
            logware::ExceptionLogData(0, "Added road to matrix", 
                "index: " + std::to_string(index) + 
                ", start: " + std::to_string(start) + 
                ", end: " + std::to_string(end)));
                
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Failed to fill matrix for road", e.what()));
        throw;
    }
}

void Roadmap::AddRoad(const Road& road) {
    try {
        const int64_t SCALLED_OFFSET = static_cast<int64_t>(OFFSET * SCALE_FACTOR_OF_CELL);
        size_t index = roads_.size();
        roads_.push_back(road);
        
        if (road.IsHorizontal()) {
            int64_t start = static_cast<int64_t>(std::min(road.GetStart().x, road.GetEnd().x));
            int64_t end = static_cast<int64_t>(std::max(road.GetStart().x, road.GetEnd().x));
            start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
            end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
            int64_t y = static_cast<int64_t>(road.GetStart().y) * SCALE_FACTOR_OF_CELL;
            
            FillMatrixForRoad(road, index, start, end, y, true);
            
            BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                logware::ExceptionLogData(0, "Added horizontal road", 
                    "index: " + std::to_string(index) + 
                    ", x range: [" + std::to_string(start) + ", " + std::to_string(end) + 
                    "], y: " + std::to_string(y)));
        } else {
            int64_t start = static_cast<int64_t>(std::min(road.GetStart().y, road.GetEnd().y));
            int64_t end = static_cast<int64_t>(std::max(road.GetStart().y, road.GetEnd().y));
            start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
            end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
            int64_t x = static_cast<int64_t>(road.GetStart().x) * SCALE_FACTOR_OF_CELL;
            
            FillMatrixForRoad(road, index, start, end, x, false);
            
            BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                logware::ExceptionLogData(0, "Added vertical road", 
                    "index: " + std::to_string(index) + 
                    ", y range: [" + std::to_string(start) + ", " + std::to_string(end) + 
                    "], x: " + std::to_string(x)));
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Failed to add road", e.what()));
        throw;
    }
}

const Roadmap::Roads& Roadmap::GetRoads() const noexcept {
    return roads_;
}

std::tuple<Position, Velocity> Roadmap::GetValidMove(const Position& old_position,
                            const Position& potential_new_position,
                            const Velocity& old_velocity) {
    try {
        Velocity velocity = {0, 0};
        auto start_roads = GetCoordinatesOfPosition(old_position);
        
        if (!start_roads) {
            BOOST_LOG_TRIVIAL(warning) << logware::CreateLogMessage("warning"sv,
                logware::ExceptionLogData(0, "Start position not on any road", 
                    "x: " + std::to_string(old_position.x) + ", y: " + std::to_string(old_position.y)));
            return std::tie(old_position, Velocity{0, 0});
        }
        
        auto end_roads = GetCoordinatesOfPosition(potential_new_position);
        
        if(end_roads){
            if(!IsValidPosition(matrix_map_[end_roads.value().x][end_roads.value().y],
                                potential_new_position)) {
                BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                    logware::ExceptionLogData(0, "End position invalid on road", 
                        "x: " + std::to_string(potential_new_position.x) + 
                        ", y: " + std::to_string(potential_new_position.y)));
                end_roads = std::nullopt;
            } else if(start_roads == end_roads) {
                BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                    logware::ExceptionLogData(0, "Move within same road cell", ""));
                return std::tie(potential_new_position, old_velocity);
            }
        }
        
        auto dest = GetDestinationRoadsOfRoute(start_roads, end_roads, old_velocity);
        Position position;
        
        if(dest && IsValidPosition(matrix_map_[dest.value().x][dest.value().y], potential_new_position)) {
            position = potential_new_position;
            velocity = old_velocity;
            
            BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                logware::ExceptionLogData(0, "Valid move to position", 
                    "x: " + std::to_string(position.x) + ", y: " + std::to_string(position.y)));
        } else {
            position = GetFarestPoinOfRoute(dest.value_or(start_roads.value()), old_position, old_velocity);
            
            BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                logware::ExceptionLogData(0, "Move stopped at position", 
                    "x: " + std::to_string(position.x) + ", y: " + std::to_string(position.y)));
        }
        
        return std::tie(position, velocity);
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in GetValidMove", e.what()));
        return std::tie(old_position, Velocity{0, 0});
    }
}

std::optional<Roadmap::MatrixMapCoord> Roadmap::GetDestinationRoadsOfRoute(
                                    std::optional<MatrixMapCoord> start,
                                    std::optional<MatrixMapCoord> end,
                                    const Velocity& old_velocity) {
    try {
        if(!start) {
            return std::nullopt;
        }
        
        const MatrixMapCoord start_coord = start.value();
        MatrixMapCoord current_coord = start_coord;
        
        if(std::abs(old_velocity.vx) > EPSILON) {
            int direction = old_velocity.vx > 0 ? 1 : -1;
            int64_t end_x = end ? end.value().x : (direction > 0 ? std::numeric_limits<int64_t>::max() : 
                                                                     std::numeric_limits<int64_t>::min());
            
            for(int64_t x = start_coord.x; direction > 0 ? x <= end_x : x >= end_x; x += direction) {
                if(!ValidateCoordinates({x, start_coord.y})) {
                    break;
                }
                
                if(!IsCrossedSets(matrix_map_[start_coord.x][start_coord.y],
                                matrix_map_[x][start_coord.y])) {
                    break;
                }
                current_coord = {x, start_coord.y};
            }
        } else if(std::abs(old_velocity.vy) > EPSILON) {
            int direction = old_velocity.vy > 0 ? 1 : -1;
            int64_t end_y = end ? end.value().y : (direction > 0 ? std::numeric_limits<int64_t>::max() : 
                                                                     std::numeric_limits<int64_t>::min());
            
            for(int64_t y = start_coord.y; direction > 0 ? y <= end_y : y >= end_y; y += direction) {
                if(!ValidateCoordinates({start_coord.x, y})) {
                    break;
                }
                
                if(!IsCrossedSets(matrix_map_[start_coord.x][start_coord.y],
                                matrix_map_[start_coord.x][y])) {
                    break;
                }
                current_coord = {start_coord.x, y};
            }
        }
        
        return current_coord;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in GetDestinationRoadsOfRoute", e.what()));
        return std::nullopt;
    }
}

std::optional<Roadmap::MatrixMapCoord> Roadmap::GetCoordinatesOfPosition(const Position& position) {
    try {
        if(position.x < -OFFSET - EPSILON || position.y < -OFFSET - EPSILON) {
            BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
                logware::ExceptionLogData(0, "Position out of bounds", 
                    "x: " + std::to_string(position.x) + ", y: " + std::to_string(position.y)));
            return std::nullopt;
        }
        
        int64_t x_index = static_cast<int64_t>(std::floor(position.x * SCALE_FACTOR_OF_CELL));
        int64_t y_index = static_cast<int64_t>(std::floor(position.y * SCALE_FACTOR_OF_CELL));
        
        auto x_it = matrix_map_.find(x_index);
        if(x_it != matrix_map_.end()) {
            if(x_it->second.find(y_index) != x_it->second.end()) {
                return MatrixMapCoord{x_index, y_index};
            }
        }
        
        return std::nullopt;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in GetCoordinatesOfPosition", e.what()));
        return std::nullopt;
    }
}

bool Roadmap::IsCrossedSets(const std::unordered_set<size_t>& lhs,
                            const std::unordered_set<size_t>& rhs) {
    try {
        for(auto item : lhs) {
            if(rhs.find(item) != rhs.end()) {
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in IsCrossedSets", e.what()));
        return false;
    }
}

bool Roadmap::ValidateCoordinates(const MatrixMapCoord& coordinates) {
    try {
        auto x_it = matrix_map_.find(coordinates.x);
        if(x_it != matrix_map_.end()) {
            return x_it->second.find(coordinates.y) != x_it->second.end();
        }
        return false;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in ValidateCoordinates", e.what()));
        return false;
    }
}

Position Roadmap::GetFarestPoinOfRoute(const MatrixMapCoord& roads_coord,
                                    const Position& old_position,
                                    const Velocity& old_velocity) {
    try {
        Position res_position{old_position};
        auto cell_pos = MatrixCoordinateToPosition(roads_coord, old_position);
        auto direction = VelocityToDirection(old_velocity);
        auto opposite = DIRECTION_TO_OPOSITE_DIRECTION.at(direction);
        
        auto road_indices = matrix_map_[roads_coord.x][roads_coord.y];
        
        for(auto road_ind : road_indices) {
            if(road_ind >= roads_.size()) {
                BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                    logware::ExceptionLogData(0, "Invalid road index", std::to_string(road_ind)));
                continue;
            }
            
            if(IsValidPositionOnRoad(roads_[road_ind], cell_pos.at(opposite))) {
                if(IsValidPositionOnRoad(roads_[road_ind], cell_pos.at(direction))) {
                    return cell_pos.at(direction);
                }
                res_position = cell_pos.at(opposite);
            }
        }
        
        return res_position;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in GetFarestPoinOfRoute", e.what()));
        return old_position;
    }
}

std::unordered_map<Direction, Position> Roadmap::MatrixCoordinateToPosition(const MatrixMapCoord& coord,
                                                                                const Position& target_position){
    std::unordered_map<Direction, Position> res;
    double cell_size = 1.0 / SCALE_FACTOR_OF_CELL;
    
    try {
        res[Direction::NORTH] = Position{
            target_position.x,
            static_cast<double>(coord.y) * cell_size
        };
        res[Direction::SOUTH] = Position{
            target_position.x,
            static_cast<double>(coord.y + 1) * cell_size
        };
        res[Direction::WEST] = Position{
            static_cast<double>(coord.x) * cell_size,
            target_position.y
        };
        res[Direction::EAST] = Position{
            static_cast<double>(coord.x + 1) * cell_size,
            target_position.y
        };
        res[Direction::NONE] = target_position;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in MatrixCoordinateToPosition", e.what()));
    }
    
    return res;
}

Direction Roadmap::VelocityToDirection(const Velocity& velocity) {
    try {
        if(std::abs(velocity.vx) > EPSILON) {
            return velocity.vx > 0 ? Direction::EAST : Direction::WEST;
        }
        if(std::abs(velocity.vy) > EPSILON) {
            return velocity.vy > 0 ? Direction::SOUTH : Direction::NORTH;
        }
        return Direction::NONE;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in VelocityToDirection", e.what()));
        return Direction::NONE;
    }
}

bool Roadmap::IsValidPosition(const std::unordered_set<size_t>& roads_ind, const Position& position) {
    try {
        for(auto road_index : roads_ind) {
            if(road_index >= roads_.size()) {
                BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                    logware::ExceptionLogData(0, "Invalid road index in IsValidPosition", 
                        std::to_string(road_index)));
                continue;
            }
            
            if(IsValidPositionOnRoad(roads_[road_index], position)) {
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in IsValidPosition", e.what()));
        return false;
    }
}

bool Roadmap::IsValidPositionOnRoad(const Road& road, const Position& position) {
    try {
        if(road.IsHorizontal()) {
            double start_x = std::min(road.GetStart().x, road.GetEnd().x) - OFFSET;
            double end_x = std::max(road.GetStart().x, road.GetEnd().x) + OFFSET;
            double y = road.GetStart().y;
            
            return (position.x >= start_x - EPSILON) &&
                   (position.x <= end_x + EPSILON) &&
                   (std::abs(position.y - y) <= OFFSET + EPSILON);
        } else {
            double start_y = std::min(road.GetStart().y, road.GetEnd().y) - OFFSET;
            double end_y = std::max(road.GetStart().y, road.GetEnd().y) + OFFSET;
            double x = road.GetStart().x;
            
            return (std::abs(position.x - x) <= OFFSET + EPSILON) &&
                   (position.y >= start_y - EPSILON) &&
                   (position.y <= end_y + EPSILON);
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in IsValidPositionOnRoad", e.what()));
        return false;
    }
}

void Roadmap::CopyContent(const Roadmap::Roads& roads) {
    try {
        for(const auto& road : roads) {
            AddRoad(road);
        }
        
        BOOST_LOG_TRIVIAL(debug) << logware::CreateLogMessage("debug"sv,
            logware::ExceptionLogData(0, "Copied roadmap content", 
                "roads count: " + std::to_string(roads.size())));
                
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Error in CopyContent", e.what()));
        throw;
    }
}

}