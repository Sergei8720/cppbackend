#pragma once
#include "player.h"
#include "dog_serialization.h"
#include "player_tokens.h"

#include <boost/serialization/vector.hpp>

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

    // Базовые конструкторы
    PlayerSerialization(const PlayerSerialization&) = default;
    PlayerSerialization(PlayerSerialization&&) = default;
    PlayerSerialization& operator=(const PlayerSerialization&) = default;
    PlayerSerialization& operator=(PlayerSerialization&&) = default;

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

}