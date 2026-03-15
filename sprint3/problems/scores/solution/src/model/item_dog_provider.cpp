#include "item_dog_provider.h"

namespace model{

size_t ItemDogProvider::ItemsCount() const {
    return items_.size();
};

collision_detector::Item ItemDogProvider::GetItem(size_t idx) const {
    return *items_.at(idx);
};

size_t ItemDogProvider::GatherersCount() const {
    return dogs_.size();
};

collision_detector::Gatherer ItemDogProvider::GetGatherer(size_t idx) const {
    return dogs_.at(idx)->AsGatherer();
};

const Dog::Id& ItemDogProvider::GetDogId(size_t idx) const {
    return dogs_.at(idx)->GetId();
};

}
