#pragma once
#include "game_session_serialization.h"
#include "player_serialization.h"
#include <boost/serialization/vector.hpp>

namespace game_data_ser {

// Общая структура для сериализации всей игры
struct GameSerialization {
    std::vector<GameSessionSerialization> sessions;
    
    // Конструктор копирования
    GameSerialization(const GameSerialization& other) = default;
    
    // Move конструктор
    GameSerialization(GameSerialization&& other) = default;
    
    // Операторы присваивания
    GameSerialization& operator=(const GameSerialization& other) = default;
    GameSerialization& operator=(GameSerialization&& other) = default;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& sessions;
    }
};

} // namespace game_data_ser