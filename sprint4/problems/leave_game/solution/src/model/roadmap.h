#pragma once

#include "road.h"
#include "velocity.h"
#include "direction.h"
#include "model_invariants.h"

#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <vector>
#include <tuple>
#include <cstdint>

namespace geom {
    struct Point2D {
        double x = 0.0;
        double y = 0.0;
    };
}

namespace model {

class Roadmap {
public:
    using Roads = std::vector<Road>;
    
    struct MatrixMapCoord {
        int64_t x;
        int64_t y;
        bool operator==(const MatrixMapCoord& other) const {
            return x == other.x && y == other.y;
        }
        bool operator!=(const MatrixMapCoord& other) const {
            return !(*this == other);
        }
    };

    Roadmap() = default;
    Roadmap(const Roadmap& other);
    Roadmap(Roadmap&& other);
    Roadmap& operator = (const Roadmap& other);
    Roadmap& operator = (Roadmap&& other);

    void AddRoad(const Road& road);
    const Roads& GetRoads() const noexcept;

    std::tuple<geom::Point2D, Velocity> GetValidMove(const geom::Point2D& old_position,
                        const geom::Point2D& potential_new_position,
                        const Velocity& old_velocity) const;
    
    geom::Point2D GenerateValidRandomPosition() const;

private:
    using MatrixMap = std::unordered_map<int64_t, std::unordered_map<int64_t, std::unordered_set<size_t>>>;
    
    MatrixMap matrix_map_;
    Roads roads_;

    // Вспомогательные методы для AddRoad
    void AddHorizontalRoad(const Road& road, int64_t scaled_offset);
    void AddVerticalRoad(const Road& road, int64_t scaled_offset);
    
    // Вспомогательные методы для движения
    std::optional<MatrixMapCoord> MoveAlongX(
        const MatrixMapCoord& start_coord,
        std::optional<MatrixMapCoord> end,
        const Velocity& old_velocity) const;
    
    std::optional<MatrixMapCoord> MoveAlongY(
        const MatrixMapCoord& start_coord,
        std::optional<MatrixMapCoord> end,
        const Velocity& old_velocity) const;

    std::optional<MatrixMapCoord> GetDestinationRoadsOfRoute(
        std::optional<MatrixMapCoord> start,
        std::optional<MatrixMapCoord> end,
        const Velocity& old_velocity) const;
    
    std::optional<MatrixMapCoord> GetCoordinatesOfPosition(const geom::Point2D& position) const;
    
    bool IsCrossedSets(const std::unordered_set<size_t>& lhs,
                       const std::unordered_set<size_t>& rhs) const;
    
    bool ValidateCoordinates(const MatrixMapCoord& coordinates) const;
    
    geom::Point2D GetFarestPoinOfRoute(const MatrixMapCoord& roads_coord,
                                       const geom::Point2D& old_position,
                                       const Velocity& old_velocity) const;
    
    std::unordered_map<Direction, geom::Point2D> MatrixCoordinateToPosition(
        const MatrixMapCoord& coord, const geom::Point2D& target_position) const;
    
    Direction VelocityToDirection(const Velocity& velocity) const;
    
    bool IsValidPosition(const std::unordered_set<size_t>& roads_ind,
                         const geom::Point2D& position) const;
    
    bool IsValidPositionOnRoad(const Road& road, const geom::Point2D& position) const;
    
    void CopyContent(const Roads& roads);
};

} // namespace model