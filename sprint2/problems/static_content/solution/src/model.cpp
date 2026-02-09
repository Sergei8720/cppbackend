#include "model.h"

#include <stdexcept>
#include <string>

namespace model {

using namespace std::literals;

void Map::AddOffice(Office office) {
  if (warehouse_id_to_index_.contains(office.GetId())) {
    throw std::invalid_argument("Duplicate warehouse");
  }

  const size_t index = offices_.size();
  Office& new_office = offices_.emplace_back(std::move(office));
  
  try {
    warehouse_id_to_index_.emplace(new_office.GetId(), index);
  } catch (...) {
    offices_.pop_back();
    throw;
  }
}

void Game::AddMap(Map map) {
  const size_t index = maps_.size();
  auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index);
  
  if (!inserted) {
    throw std::invalid_argument("Map with id "s + *map.GetId() + 
                                " already exists"s);
  }
  
  try {
    maps_.emplace_back(std::move(map));
  } catch (...) {
    map_id_to_index_.erase(it);
    throw;
  }
}

}  // namespace model