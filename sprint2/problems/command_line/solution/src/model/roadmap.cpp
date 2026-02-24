#include "roadmap.h"

#include <cmath>
#include <iostream>
#include <stdint.h>
#include <set>
#include <algorithm>

namespace model {

const int SCALE_FACTOR_OF_CELL = 20;

Roadmap::Roadmap(const Roadmap& other) {
    CopyContent(other.roads_);
}

Roadmap::Roadmap(Roadmap&& other) {
    matrix_map_ = std::move(other.matrix_map_);
    roads_ = std::move(other.roads_);
}

Roadmap& Roadmap::operator = (const Roadmap& other) {
    if(this != &other) {
        CopyContent(other.roads_);
    }
    return *this;
}

Roadmap& Roadmap::operator = (Roadmap&& other) {
    if(this != &other) {
        matrix_map_ = std::move(other.matrix_map_);
        roads_ = std::move(other.roads_);
    }
    return *this;
}

void Roadmap::AddRoad(const Road& road) {
    const auto SCALLED_OFFSET = static_cast<int64_t>(OFFSET * SCALE_FACTOR_OF_CELL);
    auto index = roads_.size();
    roads_.emplace_back(road);
    if(road.IsHorizontal()) {
        auto start = static_cast<int64_t>((road.GetStart().x < road.GetEnd().x) ? road.GetStart().x : road.GetEnd().x);
        auto end = static_cast<int64_t>((road.GetStart().x < road.GetEnd().x) ? road.GetEnd().x : road.GetStart().x);
        start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
        end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
        auto y = static_cast<int64_t>(road.GetStart().y) * SCALE_FACTOR_OF_CELL;
        for(auto x = start; x <= end; ++x) {
            for(auto i = -(SCALLED_OFFSET); i <= SCALLED_OFFSET; ++i) {
                matrix_map_[x][y + i].insert(index);
            }
        }
    } else {
        auto start = static_cast<int64_t>((road.GetStart().y < road.GetEnd().y) ? road.GetStart().y : road.GetEnd().y);
        auto end = static_cast<int64_t>((road.GetStart().y < road.GetEnd().y) ? road.GetEnd().y : road.GetStart().y);
        start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
        end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
        auto x = static_cast<int64_t>(road.GetStart().x) * SCALE_FACTOR_OF_CELL;
        for(auto y = start; y <= end; ++y) {
            for(auto i = -SCALLED_OFFSET; i <= SCALLED_OFFSET; ++i) {
                matrix_map_[x + i][y].insert(index);
            }
        }
    }
}

const Roadmap::Roads& Roadmap::GetRoads() const noexcept {
    return roads_;
}

std::tuple<Position, Velocity> Roadmap::GetValidMove(const Position& old_position,
                            const Position& potential_new_position,
                            const Velocity& old_velocity) {
    Velocity velocity = {0, 0};
    auto start_roads = GetCoordinatesOfPosition(old_position);
    auto end_roads = GetCoordinatesOfPosition(potential_new_position);
    if(end_roads){
        if(!IsValidPosition(matrix_map_[end_roads.value().x][end_roads.value().y],
                            potential_new_position)) {
            end_roads = std::nullopt;
        } else if(start_roads == end_roads) {
            return std::tie(potential_new_position, old_velocity);
        }
    }
    auto dest = GetDestinationRoadsOfRoute(start_roads, end_roads, old_velocity);
    Position position;
    if(dest && IsValidPosition(matrix_map_[dest.value().x][dest.value().y], potential_new_position)) {
        position = potential_new_position;
        velocity = old_velocity;
    } else {
        position = GetFarestPoinOfRoute(dest.value(), old_position, old_velocity);
    }
    return std::tie(position, velocity);
}

std::optional<Roadmap::MatrixMapCoord> Roadmap::GetDestinationRoadsOfRoute(
                                    std::optional<MatrixMapCoord> start,
                                    std::optional<MatrixMapCoord> end,
                                    const Velocity& old_velocity) {
    if(!start) {
        return std::nullopt;
    }
    const MatrixMapCoord start_coord = start.value();
    MatrixMapCoord current_coord = start_coord;
    if(std::abs(old_velocity.vx) > EPSILON) {
        int direction = std::signbit(old_velocity.vx) ? -1 : 1;
        int64_t end_x{0};
        if(end) {
            end_x = end.value().x;
            end_x = (direction > 0) ? (end_x < LLONG_MAX ? end_x + 1 : LLONG_MAX) :
                                        end_x - 1;
        } else {
            end_x = (direction > 0) ? LLONG_MAX : -(OFFSET * SCALE_FACTOR_OF_CELL) - 1;
        }
        int64_t index{0};
        for(index = start_coord.x; index != end_x; index += direction) {
            if(ValidateCoordinates({index, start_coord.y}) &&
                IsCrossedSets(matrix_map_[start_coord.x][start_coord.y],
                                matrix_map_[index][start_coord.y])) {
                current_coord = {index, start_coord.y};
            } else {
                break;
            }
        }
        return current_coord;
    } else if(std::abs(old_velocity.vy) > EPSILON) {
        int direction = std::signbit(old_velocity.vy) ? -1 : 1;
        int64_t end_y{0};
        if(end) {
            end_y = end.value().y;
            end_y = (direction > 0) ? (end_y < LLONG_MAX ? end_y + 1 : LLONG_MAX) :
                                        end_y - 1;
        } else {
            end_y = (direction > 0) ? LLONG_MAX : -(OFFSET * SCALE_FACTOR_OF_CELL)  - 1;
        }
        int64_t index{0};
        for(index = start_coord.y; index != end_y; index += direction) {
            if(ValidateCoordinates({start_coord.x, index}) &&
                IsCrossedSets(matrix_map_[start_coord.x][start_coord.y],
                                matrix_map_[start_coord.x][index])) {
                current_coord =  {start_coord.x, index};
            } else {
                break;
            }
        }
        return current_coord;
    }
    return std::nullopt;
}

std::optional<Roadmap::MatrixMapCoord> Roadmap::GetCoordinatesOfPosition(const Position& position) {
    if(position.x < -OFFSET - EPSILON || position.y < -OFFSET - EPSILON) {
        return std::nullopt;
    }
    int64_t x_index = (position.x >= 0) ? static_cast<int64_t>(std::floor(position.x * SCALE_FACTOR_OF_CELL)) : 
                                         static_cast<int64_t>(std::ceil(position.x * SCALE_FACTOR_OF_CELL));
    int64_t y_index = (position.y >= 0) ? static_cast<int64_t>(std::floor(position.y * SCALE_FACTOR_OF_CELL)) : 
                                         static_cast<int64_t>(std::ceil(position.y * SCALE_FACTOR_OF_CELL));
    auto x_it = matrix_map_.find(x_index);
    if(x_it != matrix_map_.end()) {
        if(x_it->second.find(y_index) != x_it->second.end()) {
            return MatrixMapCoord{x_index, y_index};
        }
    }
    return std::nullopt;
}

bool Roadmap::IsCrossedSets(const std::unordered_set<size_t>& lhs,
                            const std::unordered_set<size_t>& rhs) {
    for(auto item : lhs) {
        if(rhs.find(item) != rhs.end()) {
            return true;
        }
    }
    return false;
}

bool Roadmap::ValidateCoordinates(const MatrixMapCoord& coordinates) {
    auto x_it = matrix_map_.find(coordinates.x);
    if(x_it != matrix_map_.end()) {
        return x_it->second.find(coordinates.y) != x_it->second.end();
    }
    return false;
}

Position Roadmap::GetFarestPoinOfRoute(const MatrixMapCoord& roads_coord,
                                    const Position& old_position,
                                    const Velocity& old_velocity) {
    Position res_position{old_position};
    auto cell_pos = MatrixCoordinateToPosition(roads_coord, old_position);
    auto direction = VelocityToDirection(old_velocity);
    auto opposite = DIRECTION_TO_OPOSITE_DIRECTION.at(direction);
    for(auto road_ind : matrix_map_[roads_coord.x][roads_coord.y]) {
        if(IsValidPositionOnRoad(roads_[road_ind], cell_pos.at(opposite))) {
            if(IsValidPositionOnRoad(roads_[road_ind], cell_pos.at(direction))) {
                return cell_pos.at(direction);
            }
            res_position = cell_pos.at(opposite);
        }
    }
    return res_position;
}

std::unordered_map<Direction, Position> Roadmap::MatrixCoordinateToPosition(const MatrixMapCoord& coord,
                                                                                const Position& target_position){
    std::unordered_map<Direction, Position> res;
    double cell_size = 1.0 / SCALE_FACTOR_OF_CELL;
    
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
    res[Direction::NONE] = Position{target_position.x, target_position.y};
    return res;
}

Direction Roadmap::VelocityToDirection(const Velocity& velocity) {
    if(std::abs(velocity.vx) > EPSILON) {
        return velocity.vx > 0 ? Direction::EAST : Direction::WEST;
    }
    if(std::abs(velocity.vy) > EPSILON) {
        return velocity.vy > 0 ? Direction::SOUTH : Direction::NORTH;
    }
    return Direction::NONE;
}

bool Roadmap::IsValidPosition(const std::unordered_set<size_t>& roads_ind, const Position& position) {
    for(auto road_index : roads_ind) {
        if(IsValidPositionOnRoad(roads_[road_index], position)) {
            return true;
        }
    }
    return false;
}

bool Roadmap::IsValidPositionOnRoad(const Road& road, const Position& position) {
    if(road.IsHorizontal()) {
        double start_x = (road.GetStart().x < road.GetEnd().x) ? (road.GetStart().x) : (road.GetEnd().x);
        double end_x = (road.GetStart().x < road.GetEnd().x) ? (road.GetEnd().x) : (road.GetStart().x);
        double y = road.GetStart().y;

        start_x -= OFFSET;
        end_x += OFFSET;

        return (position.x > start_x - EPSILON) &&
               (position.x < end_x + EPSILON) &&
               (std::abs(position.y - y) < OFFSET + EPSILON);
    } else {
        double start_y = (road.GetStart().y < road.GetEnd().y) ? (road.GetStart().y) : (road.GetEnd().y);
        double end_y = (road.GetStart().y < road.GetEnd().y) ? (road.GetEnd().y) : (road.GetStart().y);
        double x = road.GetStart().x;

        start_y -= OFFSET;
        end_y += OFFSET;

        return (std::abs(position.x - x) < OFFSET + EPSILON) &&
               (position.y > start_y - EPSILON) &&
               (position.y < end_y + EPSILON);
    }
}

void Roadmap::CopyContent(const Roadmap::Roads& roads) {
    for(const auto& road : roads) {
        AddRoad(road);
    }
};

}