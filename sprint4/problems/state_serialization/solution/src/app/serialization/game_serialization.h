#pragma once
#include "game_session_serialization.h"
#include "player_serialization.h"
#include <boost/serialization/vector.hpp>

namespace game_data_ser {

// Общая структура для сериализации всей игры
struct GameSerialization {
    std::vector<GameSessionSerialization> sessions;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& sessions;
    }
};

} // namespace game_data_ser