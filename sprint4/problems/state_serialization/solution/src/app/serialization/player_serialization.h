#pragma once
#include "player.h"
#include "dog_serialization.h"
#include "player_tokens.h"

#include <boost/serialization/vector.hpp>

namespace game_data_ser {

class PlayerSerialization {
public:
    PlayerSerialization() = default;
    PlayerSerialization(app::Player& player, const authentication::Token& token):
        id_(*player.GetId()),
        name_(player.GetName()),
        token_(*token) {
        auto dog = player.GetDog().lock();
        if (dog) {
            dog_ser_ = DogSerialization(*dog);
        }
    };
    
    // Конструктор копирования
    PlayerSerialization(const PlayerSerialization& other) = default;
    
    // Move конструктор
    PlayerSerialization(PlayerSerialization&& other) = default;
    
    // Операторы присваивания
    PlayerSerialization& operator=(const PlayerSerialization& other) = default;
    PlayerSerialization& operator=(PlayerSerialization&& other) = default;

    [[nodiscard]] app::Player Restore() const {
        return app::Player(app::Player::Id{id_}, name_);
    };
    
    [[nodiscard]] model::Dog RestoreDog() const {
        return dog_ser_.Restore();
    };
    
    [[nodiscard]] authentication::Token RestoreToken() const {
        return authentication::Token(token_);
    };

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& dog_ser_;
        ar& token_;
    }
    
    // Геттеры
    size_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    const std::string& GetToken() const { return token_; }
    
private:
    size_t id_;
    std::string name_;
    DogSerialization dog_ser_;
    std::string token_;
};

}