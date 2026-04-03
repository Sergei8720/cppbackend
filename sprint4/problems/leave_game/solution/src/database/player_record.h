#pragma once
#include <string>
#include <cstdint>

namespace database {

struct PlayerRecord {
    int64_t id_uuid;      // ИЗМЕНЕНО: std::string -> int64_t (ID собаки)
    std::string name;      // Кличка собаки
    int64_t score;         // Набранные очки
    int64_t play_time_ms;  // Время в игре в миллисекундах
    
    PlayerRecord() = default;
    
    PlayerRecord(int64_t uuid,              // ИЗМЕНЕНО: const std::string& -> int64_t
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