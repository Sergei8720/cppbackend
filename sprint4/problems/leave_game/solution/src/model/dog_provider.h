#pragma once
#include "collision_detector.h"
#include "lost_object.h"
#include "dog.h"

#include <unordered_map>
#include <memory>
#include <stdexcept>

namespace model {

template<typename T>
class DogProvider: public collision_detector::ItemGathererProvider {
public:
    using Id = typename T::Id;
    using ItemIdHasher = util::TaggedHasher<Id>;
    using Items = std::unordered_map<Id,
                                    std::shared_ptr<T>,
                                    ItemIdHasher>;
    using DogIdHasher = util::TaggedHasher<Dog::Id>;
    using Dogs = std::unordered_map<Dog::Id,
                                    std::shared_ptr<Dog>,
                                    DogIdHasher>;

    DogProvider(const Items& items,
                            const Dogs& dogs) {
        for(auto [id, element] : items) {
            items_.push_back(element);
        }
        for(auto [id, dog] : dogs) {
            if(dog->IsFullBag()) {
                continue;
            }
            dogs_.push_back(dog);
        }
    };

    virtual ~DogProvider() = default;
    
    size_t ItemsCount() const override {
        return items_.size();
    };

    collision_detector::Item GetItem(size_t idx) const override {
        if (idx >= items_.size()) {
            throw std::out_of_range("GetItem: index out of range");
        }
        return items_[idx]->AsItem();
    };
    
    size_t GatherersCount() const override {
        return dogs_.size();
    }; 

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        if (idx >= dogs_.size()) {
            throw std::out_of_range("GetGatherer: index out of range");
        }
        return dogs_[idx]->AsGatherer();
    };

    const Id& GetItemId(size_t idx) const {
        if (idx >= items_.size()) {
            throw std::out_of_range("GetItemId: index out of range");
        }
        return items_[idx]->GetId();
    };

    const Dog::Id& GetDogId(size_t idx) const {
        if (idx >= dogs_.size()) {
            throw std::out_of_range("GetDogId: index out of range");
        }
        return dogs_[idx]->GetId();
    };

private:
    std::vector< std::shared_ptr<T> > items_;
    std::vector< std::shared_ptr<Dog> > dogs_;
};

}