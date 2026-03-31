#pragma once
#include "player.h"
#include "dog_serialization.h"
#include "player_tokens.h"

#include <boost/serialization/vector.hpp>

namespace game_data_ser {

class PlayerSerialization {
public:
    PlayerSerialization() = default;
    
    PlayerSerialization(const app::Player& player, const authentication::Token& token)
        : id_(*player.GetId())
        , name_(player.GetName())
        , token_(*token)
        , join_time_(player.GetJoinTime()) {
        auto dog_ptr = player.GetDog().lock();
        if (dog_ptr) {
            dog_ser_ = DogSerialization(*dog_ptr);
        }
    }

    // Конструктор копирования
    PlayerSerialization(const PlayerSerialization& other)
        : id_(other.id_)
        , name_(other.name_)
        , dog_ser_(other.dog_ser_)
        , token_(other.token_)
        , join_time_(other.join_time_) {}

    // Оператор присваивания копированием
    PlayerSerialization& operator=(const PlayerSerialization& other) {
        if (this != &other) {
            id_ = other.id_;
            name_ = other.name_;
            dog_ser_ = other.dog_ser_;
            token_ = other.token_;
            join_time_ = other.join_time_;
        }
        return *this;
    }

    // Конструктор перемещения
    PlayerSerialization(PlayerSerialization&& other) noexcept
        : id_(std::move(other.id_))
        , name_(std::move(other.name_))
        , dog_ser_(std::move(other.dog_ser_))
        , token_(std::move(other.token_))
        , join_time_(std::move(other.join_time_)) {}

    // Оператор присваивания перемещением
    PlayerSerialization& operator=(PlayerSerialization&& other) noexcept {
        if (this != &other) {
            id_ = std::move(other.id_);
            name_ = std::move(other.name_);
            dog_ser_ = std::move(other.dog_ser_);
            token_ = std::move(other.token_);
            join_time_ = std::move(other.join_time_);
        }
        return *this;
    }

    [[nodiscard]] app::Player Restore() const {
        app::Player player(app::Player::Id{id_}, name_);
        player.SetJoinTime(join_time_);
        return player;
    }
    
    [[nodiscard]] model::Dog RestoreDog() const {
        return dog_ser_.Restore();
    }
    
    [[nodiscard]] authentication::Token RestoreToken() const {
        return authentication::Token(token_);
    }
    
    [[nodiscard]] std::chrono::steady_clock::time_point GetJoinTime() const {
        return join_time_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& dog_ser_;
        ar& token_;
        ar& join_time_;
    }
    
private:
    size_t id_;
    std::string name_;
    DogSerialization dog_ser_;
    std::string token_;
    std::chrono::steady_clock::time_point join_time_;
};

} // namespace game_data_ser