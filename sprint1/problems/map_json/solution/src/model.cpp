#include "model.h"

#include <stdexcept>
#include <utility>

namespace model {

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate office");
    }

    const size_t index = offices_.size();
    offices_.emplace_back(std::move(office));
    
    try {
        warehouse_id_to_index_.emplace(offices_.back().GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index);
    
    if (!inserted) {
        throw std::invalid_argument("Map with id " + *map.GetId() + " already exists");
    }
    
    try {
        maps_.emplace_back(std::move(map));
    } catch (...) {
        map_id_to_index_.erase(it);
        throw;
    }
}

}  // namespace model