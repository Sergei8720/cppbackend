#pragma once
#include "application.h"
#include "game_serialization.h"
#include "ticker.h"
#include "logger.h"
#include <filesystem>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>

namespace app {

namespace net = boost::asio;

struct GameState {
    std::vector<game_data_ser::GameSessionSerialization> sessions;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& sessions;
    }
};

class StateSerializer : public std::enable_shared_from_this<StateSerializer> {
public:
    StateSerializer(Application& application,
                    const std::filesystem::path& state_file,
                    std::chrono::milliseconds save_period);
    
    void SaveState();
    bool LoadState(net::io_context& ioc);
    void StartPeriodicSaving(net::io_context& ioc);
    void SetSavingSettings(const saving::SavingSettings& settings);

private:
    Application& app_;
    std::filesystem::path state_file_;
    std::chrono::milliseconds save_period_;
    std::shared_ptr<time_m::Ticker> save_ticker_;
    std::string temp_file_;
    saving::SavingSettings saving_settings_;
};

} // namespace app