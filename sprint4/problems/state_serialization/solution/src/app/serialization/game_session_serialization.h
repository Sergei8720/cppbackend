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
        app::GameSession& game_session,
        const std::unordered_map<authentication::Token, std::shared_ptr<app::Player>,
                                 authentication::TokenHasher>& tokenToPlayer)
        : map_id_(*(game_session.GetMap()->GetId())) {
        std::ranges::transform(tokenToPlayer, std::back_inserter(players_ser_),
            [](const auto& token_to_player) -> PlayerSerialization {
                return PlayerSerialization(*token_to_player.second, token_to_player.first);
            });
        std::ranges::transform(game_session.GetLostObjects(), std::back_inserter(lost_objects_),
            [](const auto& id_to_lost_object) -> LostObjectSerialization {
                return LostObjectSerialization(*id_to_lost_object.second);
            });
    }

    // Конструктор копирования
    GameSessionSerialization(const GameSessionSerialization& other) = default;
    
    // Move конструктор
    GameSessionSerialization(GameSessionSerialization&& other) = default;
    
    // Операторы присваивания
    GameSessionSerialization& operator=(const GameSessionSerialization& other) = default;
    GameSessionSerialization& operator=(GameSessionSerialization&& other) = default;

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

} // namespace game_data_ser

BOOST_CLASS_TRACKING(game_data_ser::GameSessionSerialization, boost::serialization::track_never)