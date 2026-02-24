#pragma once
#include "road.h"
#include "building.h"
#include "office.h"
#include "support_types.h"
#include "tagged.h"
#include <memory>
#include <vector>
#include <tuple>

namespace model {

class IMap {
public:
    using Id = util::Tagged<std::string, IMap>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;
    
    virtual ~IMap() = default;
    
    virtual const Id& GetId() const noexcept = 0;
    virtual const std::string& GetName() const noexcept = 0;
    virtual const Buildings& GetBuildings() const noexcept = 0;
    virtual const Roads& GetRoads() const noexcept = 0;
    virtual const Offices& GetOffices() const noexcept = 0;
    virtual void AddRoad(const Road& road) = 0;
    virtual void AddBuilding(const Building& building) = 0;
    virtual void AddOffice(const Office& office) = 0;
    virtual void SetDogVelocity(double velocity) = 0;
    virtual double GetDogVelocity() const noexcept = 0;
    virtual std::tuple<Position, Velocity> GetValidMove(
        const Position& old_position,
        const Position& potential_new_position,
        const Velocity& old_velocity) = 0;
};

}