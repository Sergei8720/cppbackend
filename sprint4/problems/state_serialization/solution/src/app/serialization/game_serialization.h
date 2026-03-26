#pragma once
#include "game_session_serialization.h"
#include "player_serialization.h"
#include <boost/serialization/vector.hpp>

namespace game_data_ser {

struct GameSerialization {
    std::vector<GameSessionSerialization> sessions;
    
    // Конструкторы
    GameSerialization() = default;
    GameSerialization(const GameSerialization& other) = default;
    GameSerialization(GameSerialization&& other) = default;
    GameSerialization& operator=(const GameSerialization& other) = default;
    GameSerialization& operator=(GameSerialization&& other) = default;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& sessions;
    }
};

} // namespace game_data_ser

BOOST_CLASS_TRACKING(game_data_ser::GameSerialization, boost::serialization::track_never)