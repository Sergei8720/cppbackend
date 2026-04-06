#pragma once
#include <string>
#include <cstdint>

namespace database {

struct PlayerRecord {
    int64_t id_uuid{0};
    std::string name;
    int64_t score{0};
    int64_t play_time_ms{0};
    
    PlayerRecord() = default;
    
    PlayerRecord(int64_t uuid,
                 const std::string& player_name, 
                 int64_t player_score, 
                 int64_t player_play_time_ms)
        : id_uuid(uuid)
        , name(player_name)
        , score(player_score)
        , play_time_ms(player_play_time_ms) {
    }
    
    double GetPlayTimeSeconds() const {
        return static_cast<double>(play_time_ms) / 1000.0;
    }
	
	domain::PlayerRecord ToDomain() const {
		return domain::PlayerRecord(name, score, play_time_ms);
	}
};

}