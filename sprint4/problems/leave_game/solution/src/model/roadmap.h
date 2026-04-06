#pragma once

#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <vector>
#include <tuple>
#include <cstdint>
#include <cmath>

namespace geom {
    struct Point2D {
        double x = 0.0;
        double y = 0.0;
    };
}

namespace model {

// Forward declarations
struct Velocity {
    double vx = 0.0;
    double vy = 0.0;
    
    bool operator==(const Velocity& other) const {
        return std::abs(vx - other.vx) < 1e-9 && std::abs(vy - other.vy) < 1e-9;
    }
    
    bool operator!=(const Velocity& other) const {
        return !(*this == other);
    }
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST,
    NONE
};

class Road {
public:
    struct Point {
        double x;
        double y;
    };
    
    Road(Point start, Point end) : start_(start), end_(end) {}
    
    const Point& GetStart() const { return start_; }
    const Point& GetEnd() const { return end_; }
    bool IsHorizontal() const { return std::abs(start_.y - end_.y) < 1e-9; }
    
private:
    Point start_;
    Point end_;
};

// Константы из model_invariants.h
const double OFFSET = 0.4;
const double EPSILON = 1e-9;

// Отображение Velocity в Direction
const std::unordered_map<Velocity, Direction> VELOCITY_TO_DIRECTION = {
    {{1, 0}, Direction::EAST},
    {{-1, 0}, Direction::WEST},
    {{0, 1}, Direction::NORTH},
    {{0, -1}, Direction::SOUTH},
    {{0, 0}, Direction::NONE}
};

// Отображение Direction в противоположный Direction
const std::unordered_map<Direction, Direction> DIRECTION_TO_OPOSITE_DIRECTION = {
    {Direction::NORTH, Direction::SOUTH},
    {Direction::SOUTH, Direction::NORTH},
    {Direction::WEST, Direction::EAST},
    {Direction::EAST, Direction::WEST},
    {Direction::NONE, Direction::NONE}
};

// Отображение Direction в строку
const std::unordered_map<Direction, std::string> DIRECTION_TO_STRING = {
    {Direction::NORTH, "N"},
    {Direction::SOUTH, "S"},
    {Direction::WEST, "W"},
    {Direction::EAST, "E"},
    {Direction::NONE, ""}
};

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