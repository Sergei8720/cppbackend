#pragma once
#include "game_session.h"
#include "player_serialization.h"
#include "map.h"
#include "player_tokens.h"
#include "lost_object_serialization.h"
#include <vector>
#include <boost/serialization/vector.hpp>

namespace game_data_ser {

class GameSessionSerialization {
public:
    GameSessionSerialization() = default;
    
    GameSessionSerialization(
        const app::GameSession& game_session,
        const std::unordered_map<authentication::Token, std::shared_ptr<app::Player>,
                                 authentication::TokenHasher>& tokenToPlayer)
        : map_id_(*(game_session.GetMap()->GetId())) {
        players_ser_.reserve(tokenToPlayer.size());
        for (const auto& token_to_player : tokenToPlayer) {
            players_ser_.emplace_back(*token_to_player.second, token_to_player.first);
        }
        lost_objects_.reserve(game_session.GetLostObjects().size());
        for (const auto& id_to_lost_object : game_session.GetLostObjects()) {
            lost_objects_.emplace_back(*id_to_lost_object.second);
        }
    }

    GameSessionSerialization(const GameSessionSerialization& other) = default;
    GameSessionSerialization& operator=(const GameSessionSerialization& other) = default;
    GameSessionSerialization(GameSessionSerialization&& other) noexcept = default;
    GameSessionSerialization& operator=(GameSessionSerialization&& other) noexcept = default;

    [[nodiscard]] model::Map::Id RestoreMapId() const {
        return model::Map::Id(map_id_);
    }
    
    [[nodiscard]] const std::vector<LostObjectSerialization>& GetLostObjectsSerialize() const {
        return lost_objects_;
    }
    
    [[nodiscard]] const std::vector<PlayerSerialization>& GetPlayersSerialize() const {
        return players_ser_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& map_id_;
        ar& players_ser_;
        ar& lost_objects_;
    }
    
private:
    std::string map_id_;
    std::vector<PlayerSerialization> players_ser_;
    std::vector<LostObjectSerialization> lost_objects_;
};

}