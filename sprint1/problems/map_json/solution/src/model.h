#ifndef MODEL_H_
#define MODEL_H_

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x;
    Coord y;
    
    bool operator==(const Point&) const = default;
};

struct Size {
    Dimension width;
    Dimension height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx;
    Dimension dy;
};

class Road {
 public:
    enum class Orientation { HORIZONTAL, VERTICAL };

    Road(Orientation orientation, Point start, Coord end)
        : start_{start},
          end_{orientation == Orientation::HORIZONTAL ? 
               Point{end, start.y} : Point{start.x, end}} {
    }

    bool IsHorizontal() const { return start_.y == end_.y; }
    bool IsVertical() const { return start_.x == end_.x; }
    Point GetStart() const { return start_; }
    Point GetEnd() const { return end_; }

 private:
    Point start_;
    Point end_;
};

class Building {
 public:
    explicit Building(Rectangle bounds) : bounds_{bounds} {}
    const Rectangle& GetBounds() const { return bounds_; }

 private:
    Rectangle bounds_;
};

class Office {
 public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset)
        : id_{std::move(id)},
          position_{position},
          offset_{offset} {
    }

    const Id& GetId() const { return id_; }
    Point GetPosition() const { return position_; }
    Offset GetOffset() const { return offset_; }

 private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
 public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name)
        : id_{std::move(id)},
          name_{std::move(name)} {
    }

    const Id& GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    const Buildings& GetBuildings() const { return buildings_; }
    const Roads& GetRoads() const { return roads_; }
    const Offices& GetOffices() const { return offices_; }

    void AddRoad(const Road& road) { roads_.push_back(road); }
    void AddBuilding(const Building& building) { buildings_.push_back(building); }
    void AddOffice(Office office);

 private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, std::size_t,
                                               util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    Offices offices_;
    OfficeIdToIndex warehouse_id_to_index_;
};

class Game {
 public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);
    
    const Maps& GetMaps() const { return maps_; }
    
    const Map* FindMap(const Map::Id& id) const {
        auto it = map_id_to_index_.find(id);
        return it != map_id_to_index_.end() ? &maps_[it->second] : nullptr;
    }

 private:
    using MapIdToIndex = std::unordered_map<Map::Id, std::size_t,
                                            util::TaggedHasher<Map::Id>>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
};

}  // namespace model

#endif  // MODEL_H_