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
        , join_time_ns_(player.GetJoinTime().time_since_epoch().count()) {
        auto dog_ptr = player.GetDog().lock();
        if (dog_ptr) {
            dog_ser_ = DogSerialization(*dog_ptr);
        }
    }

    PlayerSerialization(const PlayerSerialization& other) = default;
    PlayerSerialization& operator=(const PlayerSerialization& other) = default;
    PlayerSerialization(PlayerSerialization&& other) noexcept = default;
    PlayerSerialization& operator=(PlayerSerialization&& other) noexcept = default;

    [[nodiscard]] app::Player Restore() const {
        app::Player player(app::Player::Id{id_}, name_);
        player.SetJoinTime(std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(join_time_ns_)
        ));
        return player;
    }
    
    [[nodiscard]] model::Dog RestoreDog() const {
        return dog_ser_.Restore();
    }
    
    [[nodiscard]] authentication::Token RestoreToken() const {
        return authentication::Token(token_);
    }
    
    [[nodiscard]] int64_t GetJoinTimeNs() const {
        return join_time_ns_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& dog_ser_;
        ar& token_;
        ar& join_time_ns_;
    }
    
private:
    size_t id_;
    std::string name_;
    DogSerialization dog_ser_;
    std::string token_;
    int64_t join_time_ns_{0};
};

}