#pragma once
#include <string>
#include <cstdint>

namespace database {

struct PlayerRecord {
    std::string id_uuid;      // UUID собаки
    std::string name;          // Кличка собаки
    int64_t score;             // Набранные очки
    int64_t play_time_ms;      // Время в игре в миллисекундах
    
    PlayerRecord() = default;
    
    PlayerRecord(const std::string& uuid, 
                 const std::string& player_name, 
                 int64_t player_score, 
                 int64_t player_play_time_ms)
        : id_uuid(uuid)
        , name(player_name)
        , score(player_score)
        , play_time_ms(player_play_time_ms) {
    }
};

} // namespace database