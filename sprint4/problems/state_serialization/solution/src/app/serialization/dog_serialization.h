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
        std::ranges::transform(dog.GetBag(), std::back_inserter(bag_),
            [](std::shared_ptr<model::LostObject> lost_object) -> LostObjectSerialization {
                return LostObjectSerialization(*lost_object);
            });
    }

    // ✅ Явно определяем конструктор копирования
    DogSerialization(const DogSerialization& other)
        : id_(other.id_)
        , name_(other.name_)
        , direction_(other.direction_)
        , position_(other.position_)
        , score_(other.score_)
        , bag_capacity_(other.bag_capacity_)
        , bag_(other.bag_) {}

    // ✅ Явно определяем оператор присваивания копированием
    DogSerialization& operator=(const DogSerialization& other) {
        if (this != &other) {
            id_ = other.id_;
            name_ = other.name_;
            direction_ = other.direction_;
            position_ = other.position_;
            score_ = other.score_;
            bag_capacity_ = other.bag_capacity_;
            bag_ = other.bag_;
        }
        return *this;
    }

    // ✅ Явно определяем конструктор перемещения
    DogSerialization(DogSerialization&& other) noexcept
        : id_(std::move(other.id_))
        , name_(std::move(other.name_))
        , direction_(other.direction_)
        , position_(std::move(other.position_))
        , score_(other.score_)
        , bag_capacity_(other.bag_capacity_)
        , bag_(std::move(other.bag_)) {}

    // ✅ Явно определяем оператор присваивания перемещением
    DogSerialization& operator=(DogSerialization&& other) noexcept {
        if (this != &other) {
            id_ = std::move(other.id_);
            name_ = std::move(other.name_);
            direction_ = other.direction_;
            position_ = std::move(other.position_);
            score_ = other.score_;
            bag_capacity_ = other.bag_capacity_;
            bag_ = std::move(other.bag_);
        }
        return *this;
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog(model::Dog::Id{id_}, name_, bag_capacity_);
        dog.SetDirection(static_cast<model::Direction>(direction_));
        dog.SetPosition(position_);
        dog.SetScore(score_);
        std::ranges::for_each(bag_, [&dog](const LostObjectSerialization& lost_obj_ser) {
            dog.CollectLostObject(std::make_shared<model::LostObject>(lost_obj_ser.Restore()));
        });
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