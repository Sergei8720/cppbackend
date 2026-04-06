#pragma once
#include "application.h"
#include "game_serialization.h"
#include "ticker.h"
#include "logger.h"
#include "saving_settings.h"
#include <filesystem>
#include <fstream>
#include <atomic>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>

namespace app {

namespace net = boost::asio;

struct GameState {
    game_data_ser::GameSerialization data;
    uint64_t max_player_id{0};
    uint64_t max_dog_id{0};
    uint64_t max_loot_id{0};
    
    GameState() = default;
    GameState(const GameState& other) = default;
    GameState(GameState&& other) = default;
    GameState& operator=(const GameState& other) = default;
    GameState& operator=(GameState&& other) = default;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& data;
        ar& max_player_id;
        ar& max_dog_id;
        ar& max_loot_id;
    }
};

class StateSerializer : public std::enable_shared_from_this<StateSerializer> {
public:
    StateSerializer(Application& application,
                    const std::filesystem::path& state_file,
                    std::chrono::milliseconds save_period);
    
    ~StateSerializer() = default;
    
    void SaveState();
    bool LoadState(net::io_context& ioc);
    void StartPeriodicSaving(net::io_context& ioc);
    void FinalSave();

    void SaveStatePublic() { SaveState(); }

private:
    Application& app_;
    std::filesystem::path state_file_;
    std::chrono::milliseconds save_period_;
    std::shared_ptr<time_m::Ticker> save_ticker_;
    std::string temp_file_;
    saving::SavingSettings saving_settings_;
    std::atomic<bool> is_saving_{false};
    std::atomic<bool> is_final_save_done_{false};
    
    friend class Application;
};

}