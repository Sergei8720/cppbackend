#pragma once
#include "lost_object.h"
#include "misc_serialization.h"

#include <boost/serialization/vector.hpp>

namespace game_data_ser {

class LostObjectSerialization {
public:
    LostObjectSerialization() = default;
    
    LostObjectSerialization(const model::LostObject& lost_object)
        : id_(*lost_object.GetId())
        , type_(lost_object.GetType())
        , value_(lost_object.GetValue())
        , position_(lost_object.GetPosition()) {}

    // ✅ Явно определяем конструктор копирования
    LostObjectSerialization(const LostObjectSerialization& other)
        : id_(other.id_)
        , type_(other.type_)
        , value_(other.value_)
        , position_(other.position_) {}

    // ✅ Явно определяем оператор присваивания копированием
    LostObjectSerialization& operator=(const LostObjectSerialization& other) {
        if (this != &other) {
            id_ = other.id_;
            type_ = other.type_;
            value_ = other.value_;
            position_ = other.position_;
        }
        return *this;
    }

    // ✅ Явно определяем конструктор перемещения
    LostObjectSerialization(LostObjectSerialization&& other) noexcept
        : id_(std::move(other.id_))
        , type_(other.type_)
        , value_(other.value_)
        , position_(std::move(other.position_)) {}

    // ✅ Явно определяем оператор присваивания перемещением
    LostObjectSerialization& operator=(LostObjectSerialization&& other) noexcept {
        if (this != &other) {
            id_ = std::move(other.id_);
            type_ = other.type_;
            value_ = other.value_;
            position_ = std::move(other.position_);
        }
        return *this;
    }

    [[nodiscard]] model::LostObject Restore() const {
        model::LostObject lost_object(model::LostObject::Id{id_});
        lost_object.SetType(type_);
        lost_object.SetValue(value_);
        lost_object.SetPosition(position_);
        return lost_object;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& type_;
        ar& value_;
        ar& position_;
    }
    
private:
    size_t id_;
    size_t type_;
    size_t value_;
    geom::Point2D position_;
};

}