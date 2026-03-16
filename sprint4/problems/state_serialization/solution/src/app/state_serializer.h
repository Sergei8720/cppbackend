#pragma once
#include "application.h"
#include "game_serialization.h"
#include "ticker.h"
#include <filesystem>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>

namespace app {

namespace net = boost::asio;

class StateSerializer : public std::enable_shared_from_this<StateSerializer> {
public:
    // Теперь конструктор принимает Application&
    StateSerializer(Application& application,
                    const std::filesystem::path& state_file,
                    std::chrono::milliseconds save_period);
    
    void SaveState();  // без параметра
    bool LoadState(net::io_context& ioc);  // без Application&
    void StartPeriodicSaving(net::io_context& ioc);  // без Application&

private:
    Application& app_;  // ссылка на приложение
    std::filesystem::path state_file_;
    std::chrono::milliseconds save_period_;
    std::shared_ptr<time_m::Ticker> save_ticker_;
    
    void OnSaveTimer(const std::chrono::milliseconds&);  // можно удалить, если не используется
};

} // namespace app