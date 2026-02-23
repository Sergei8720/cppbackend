#include "roadmap.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace model {

const int SCALE_FACTOR_OF_CELL = 20;

Roadmap::Roadmap(const Roadmap& other) {
    roads_ = other.roads_;
    for (const auto& road : roads_) {
        AddRoad(road);
    }
}

Roadmap::Roadmap(Roadmap&& other) noexcept
    : matrix_map_(std::move(other.matrix_map_))
    , roads_(std::move(other.roads_)) {
}

Roadmap& Roadmap::operator = (const Roadmap& other) {
    if (this != &other) {
        roads_ = other.roads_;
        matrix_map_.clear();
        for (const auto& road : roads_) {
            AddRoad(road);
        }
    }
    return *this;
}

Roadmap& Roadmap::operator = (Roadmap&& other) noexcept {
    if (this != &other) {
        matrix_map_ = std::move(other.matrix_map_);
        roads_ = std::move(other.roads_);
    }
    return *this;
}

void Roadmap::AddRoad(const Road& road) {
    const int64_t SCALLED_OFFSET = static_cast<int64_t>(OFFSET * SCALE_FACTOR_OF_CELL);
    size_t index = roads_.size();
    roads_.push_back(road);
    
    if (road.IsHorizontal()) {
        int64_t start = static_cast<int64_t>(std::min(road.GetStart().x, road.GetEnd().x));
        int64_t end = static_cast<int64_t>(std::max(road.GetStart().x, road.GetEnd().x));
        start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
        end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
        int64_t y = static_cast<int64_t>(road.GetStart().y) * SCALE_FACTOR_OF_CELL;
        
        for (int64_t x = start; x <= end; ++x) {
            for (int64_t i = -SCALLED_OFFSET; i <= SCALLED_OFFSET; ++i) {
                matrix_map_[x][y + i].insert(index);
            }
        }
    } else {
        int64_t start = static_cast<int64_t>(std::min(road.GetStart().y, road.GetEnd().y));
        int64_t end = static_cast<int64_t>(std::max(road.GetStart().y, road.GetEnd().y));
        start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
        end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
        int64_t x = static_cast<int64_t>(road.GetStart().x) * SCALE_FACTOR_OF_CELL;
        
        for (int64_t y = start; y <= end; ++y) {
            for (int64_t i = -SCALLED_OFFSET; i <= SCALLED_OFFSET; ++i) {
                matrix_map_[x + i][y].insert(index);
            }
        }
    }
}

const Roadmap::Roads& Roadmap::GetRoads() const noexcept {
    return roads_;
}

std::tuple<Position, Velocity> Roadmap::GetValidMove(
    const Position& old_position,
    const Position& potential_new_position,
    const Velocity& old_velocity) {
    
    auto start_roads = GetCoordinatesOfPosition(old_position);
    if (!start_roads) {
        return std::tie(old_position, Velocity{0, 0});
    }
    
    auto end_roads = GetCoordinatesOfPosition(potential_new_position);
    if (end_roads && *start_roads == *end_roads) {
        return std::tie(potential_new_position, old_velocity);
    }
    
    auto dest = GetDestinationRoadsOfRoute(start_roads, end_roads, old_velocity);
    if (dest && IsValidPosition(matrix_map_[dest->x][dest->y], potential_new_position)) {
        return std::tie(potential_new_position, old_velocity);
    }
    
    Position position = GetFarestPointOfRoute(*dest, old_position, old_velocity);
    return std::tie(position, Velocity{0, 0});
}

std::optional<Roadmap::MatrixMapCoord> Roadmap::GetDestinationRoadsOfRoute(
    std::optional<MatrixMapCoord> start,
    std::optional<MatrixMapCoord> end,
    const Velocity& old_velocity) {
    
    if (!start) {
        return std::nullopt;
    }
    
    MatrixMapCoord current_coord = *start;
    
    if (std::abs(old_velocity.vx) > EPSILON) {
        int direction = old_velocity.vx > 0 ? 1 : -1;
        int64_t end_x = end ? end->x : (direction > 0 ? std::numeric_limits<int64_t>::max() : std::numeric_limits<int64_t>::min());
        
        for (int64_t x = start->x; x != end_x; x += direction) {
            if (!ValidateCoordinates({x, start->y}) ||
                !IsCrossedSets(matrix_map_[start->x][start->y], matrix_map_[x][start->y])) {
                break;
            }
            current_coord = {x, start->y};
        }
    } else if (std::abs(old_velocity.vy) > EPSILON) {
        int direction = old_velocity.vy > 0 ? 1 : -1;
        int64_t end_y = end ? end->y : (direction > 0 ? std::numeric_limits<int64_t>::max() : std::numeric_limits<int64_t>::min());
        
        for (int64_t y = start->y; y != end_y; y += direction) {
            if (!ValidateCoordinates({start->x, y}) ||
                !IsCrossedSets(matrix_map_[start->x][start->y], matrix_map_[start->x][y])) {
                break;
            }
            current_coord = {start->x, y};
        }
    }
    
    return current_coord;
}

std::optional<Roadmap::MatrixMapCoord> Roadmap::GetCoordinatesOfPosition(const Position& position) {
    int64_t x_index = static_cast<int64_t>(std::floor(position.x * SCALE_FACTOR_OF_CELL));
    int64_t y_index = static_cast<int64_t>(std::floor(position.y * SCALE_FACTOR_OF_CELL));
    
    auto x_it = matrix_map_.find(x_index);
    if (x_it != matrix_map_.end()) {
        auto y_it = x_it->second.find(y_index);
        if (y_it != x_it->second.end()) {
            return MatrixMapCoord{x_index, y_index};
        }
    }
    return std::nullopt;
}

bool Roadmap::IsCrossedSets(const std::unordered_set<size_t>& lhs,
                           const std::unordered_set<size_t>& rhs) {
    for (auto item : lhs) {
        if (rhs.count(item)) {
            return true;
        }
    }
    return false;
}

bool Roadmap::ValidateCoordinates(const MatrixMapCoord& coordinates) {
    auto x_it = matrix_map_.find(coordinates.x);
    return x_it != matrix_map_.end() && x_it->second.count(coordinates.y) > 0;
}

Position Roadmap::GetFarestPointOfRoute(const MatrixMapCoord& roads_coord,
                                       const Position& old_position,
                                       const Velocity& old_velocity) {
    Position res_position = old_position;
    auto cell_pos = MatrixCoordinateToPosition(roads_coord, old_position);
    auto direction = VelocityToDirection(old_velocity);
    auto opposite = DIRECTION_TO_OPOSITE_DIRECTION.at(direction);
    
    for (auto road_ind : matrix_map_[roads_coord.x][roads_coord.y]) {
        if (IsValidPositionOnRoad(roads_[road_ind], cell_pos.at(opposite))) {
            if (IsValidPositionOnRoad(roads_[road_ind], cell_pos.at(direction))) {
                return cell_pos.at(direction);
            }
            res_position = cell_pos.at(opposite);
        }
    }
    return res_position;
}

std::unordered_map<Direction, Position> Roadmap::MatrixCoordinateToPosition(
    const MatrixMapCoord& coord,
    const Position& target_position) {
    
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
    res[Direction::NONE] = target_position;
    
    return res;
}

Direction Roadmap::VelocityToDirection(const Velocity& velocity) {
    if (std::abs(velocity.vx) > EPSILON) {
        return velocity.vx > 0 ? Direction::EAST : Direction::WEST;
    }
    if (std::abs(velocity.vy) > EPSILON) {
        return velocity.vy > 0 ? Direction::SOUTH : Direction::NORTH;
    }
    return Direction::NONE;
}

bool Roadmap::IsValidPosition(const std::unordered_set<size_t>& roads_ind,
                             const Position& position) {
    for (auto road_index : roads_ind) {
        if (IsValidPositionOnRoad(roads_[road_index], position)) {
            return true;
        }
    }
    return false;
}

bool Roadmap::IsValidPositionOnRoad(const Road& road, const Position& position) {
    if (road.IsHorizontal()) {
        double start_x = std::min(road.GetStart().x, road.GetEnd().x) - OFFSET;
        double end_x = std::max(road.GetStart().x, road.GetEnd().x) + OFFSET;
        double y = road.GetStart().y;
        
        return position.x >= start_x - EPSILON &&
               position.x <= end_x + EPSILON &&
               std::abs(position.y - y) <= OFFSET + EPSILON;
    } else {
        double start_y = std::min(road.GetStart().y, road.GetEnd().y) - OFFSET;
        double end_y = std::max(road.GetStart().y, road.GetEnd().y) + OFFSET;
        double x = road.GetStart().x;
        
        return position.y >= start_y - EPSILON &&
               position.y <= end_y + EPSILON &&
               std::abs(position.x - x) <= OFFSET + EPSILON;
    }
}

}