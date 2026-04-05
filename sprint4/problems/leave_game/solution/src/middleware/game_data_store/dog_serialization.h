#pragma once
#include "dog.h"
#include "lost_object_serialization.h"
#include <algorithm>
#include <boost/serialization/vector.hpp>

namespace game_data_ser {

class DogSerialization {
public:
    DogSerialization() = default;
    
    DogSerialization(const model::Dog& dog)
        : id_(*dog.GetId())
        , name_(dog.GetName())
        , direction_(static_cast<int>(dog.GetDirection()))
        , position_(dog.GetPosition())
        , score_(dog.GetScore())
        , bag_capacity_(dog.GetBagCapacity()) {
        std::transform(dog.GetBag().begin(), dog.GetBag().end(), std::back_inserter(bag_),
            [](std::shared_ptr<model::LostObject> lost_object) -> LostObjectSerialization {
                return LostObjectSerialization(*lost_object);
            });
    }

    DogSerialization(const DogSerialization& other) = default;
    DogSerialization& operator=(const DogSerialization& other) = default;
    DogSerialization(DogSerialization&& other) noexcept = default;
    DogSerialization& operator=(DogSerialization&& other) noexcept = default;

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog(model::Dog::Id{id_}, name_, bag_capacity_);
        dog.SetDirection(static_cast<model::Direction>(direction_));
        dog.SetPosition(position_);
        dog.SetScore(score_);
        for (const auto& lost_obj_ser : bag_) {
            dog.CollectLostObject(std::make_shared<model::LostObject>(lost_obj_ser.Restore()));
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& direction_;
        ar& position_;
        ar& bag_;
        ar& score_;
        ar& bag_capacity_;
    }
    
private:
    size_t id_;
    std::string name_;
    int direction_;
    geom::Point2D position_{0.0, 0.0};
    size_t score_{0};
    size_t bag_capacity_{0};
    std::vector<LostObjectSerialization> bag_;
};

}