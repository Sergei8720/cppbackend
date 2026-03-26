#pragma once
#include "player.h"
#include "dog_serialization.h"
#include "player_tokens.h"

#include <boost/serialization/vector.hpp>
#include <memory>

namespace game_data_ser {

class PlayerSerialization {
public:
    PlayerSerialization() = default;
    
    PlayerSerialization(app::Player& player, const authentication::Token& token)
        : id_(*player.GetId())
        , name_(player.GetName())
        , token_(*token) {
        auto dog_ptr = player.GetDog().lock();
        if (dog_ptr) {
            dog_ser_ = DogSerialization(*dog_ptr);
        }
    }
    
    // Конструктор копирования - явно реализован
    PlayerSerialization(const PlayerSerialization& other)
        : id_(other.id_)
        , name_(other.name_)
        , dog_ser_(other.dog_ser_)
        , token_(other.token_) {}
    
    // Move конструктор
    PlayerSerialization(PlayerSerialization&& other) noexcept
        : id_(std::move(other.id_))
        , name_(std::move(other.name_))
        , dog_ser_(std::move(other.dog_ser_))
        , token_(std::move(other.token_)) {}
    
    // Оператор присваивания копированием
    PlayerSerialization& operator=(const PlayerSerialization& other) {
        if (this != &other) {
            id_ = other.id_;
            name_ = other.name_;
            dog_ser_ = other.dog_ser_;
            token_ = other.token_;
        }
        return *this;
    }
    
    // Оператор присваивания перемещением
    PlayerSerialization& operator=(PlayerSerialization&& other) noexcept {
        if (this != &other) {
            id_ = std::move(other.id_);
            name_ = std::move(other.name_);
            dog_ser_ = std::move(other.dog_ser_);
            token_ = std::move(other.token_);
        }
        return *this;
    }

    // Возвращаем объект по значению - у app::Player должен быть конструктор копирования
    [[nodiscard]] app::Player Restore() const {
        return app::Player(app::Player::Id{id_}, name_);
    }
    
    [[nodiscard]] model::Dog RestoreDog() const {
        return dog_ser_.Restore();
    }
    
    [[nodiscard]] authentication::Token RestoreToken() const {
        return authentication::Token(token_);
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& dog_ser_;
        ar& token_;
    }
    
private:
    size_t id_;
    std::string name_;
    DogSerialization dog_ser_;
    std::string token_;
};

} // namespace game_data_ser

// Добавляем макрос для отключения отслеживания
BOOST_CLASS_TRACKING(game_data_ser::PlayerSerialization, boost::serialization::track_never)