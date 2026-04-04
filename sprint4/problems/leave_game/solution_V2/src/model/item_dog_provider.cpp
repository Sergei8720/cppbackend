#include "item_dog_provider.h"
#include <stdexcept>

namespace model{

size_t ItemDogProvider::ItemsCount() const {
    return items_.size();
};

collision_detector::Item ItemDogProvider::GetItem(size_t idx) const {
    if (idx >= items_.size()) {
        throw std::out_of_range("GetItem: index out of range");
    }
    return *items_[idx];
};

size_t ItemDogProvider::GatherersCount() const {
    return dogs_.size();
};

collision_detector::Gatherer ItemDogProvider::GetGatherer(size_t idx) const {
    if (idx >= dogs_.size()) {
        throw std::out_of_range("GetGatherer: index out of range");
    }
    return dogs_[idx]->AsGatherer();
};

const Dog::Id& ItemDogProvider::GetDogId(size_t idx) const {
    if (idx >= dogs_.size()) {
        throw std::out_of_range("GetDogId: index out of range");
    }
    return dogs_[idx]->GetId();
};

}