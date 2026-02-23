#pragma once
#include "map.h"
#include <vector>
#include <memory>

namespace model {

class IGame {
public:
    using Maps = std::vector<std::shared_ptr<Map>>;
    
    virtual ~IGame() = default;
    
    virtual void AddMap(const Map& map) = 0;
    virtual void AddMaps(const std::vector<Map>& maps) = 0;
    virtual const Maps& GetMaps() const noexcept = 0;
    virtual const std::shared_ptr<Map> FindMap(const Map::Id& id) const noexcept = 0;
    virtual void SetDefaultDogVelocity(double velocity) = 0;
    virtual double GetDefaultDogVelocity() const noexcept = 0;
};

}