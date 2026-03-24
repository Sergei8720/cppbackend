#include "state_serializer.h"
#include <thread>

namespace app {

using namespace std::literals;

StateSerializer::StateSerializer(Application& application,
                                 const std::filesystem::path& state_file,
                                 std::chrono::milliseconds save_period)
    : app_(application)
    , state_file_(state_file)
    , save_period_(save_period) {
    saving_settings_.state_file_path = state_file.string();
    saving_settings_.period = save_period;
    app_.SetSavingSettings(saving_settings_);
    
    BOOST_LOG_TRIVIAL(info) << "StateSerializer initialized: state_file=" << state_file.string()
                            << ", save_period=" << save_period.count() << "ms";
}

void StateSerializer::FinalSave() {
    if (is_final_save_done_.exchange(true)) {
        BOOST_LOG_TRIVIAL(warning) << "FinalSave: already done, skipping";
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "FinalSave: performing final save to " << state_file_.string();
    SaveState();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void StateSerializer::SaveState() {
    if (state_file_.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "SaveState: state_file is empty, skipping";
        return;
    }
    
    bool expected = false;
    if (!is_saving_.compare_exchange_strong(expected, true)) {
        BOOST_LOG_TRIVIAL(warning) << "SaveState: already in progress, skipping";
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "SaveState: saving to " << state_file_.string();
    
    try {
        temp_file_ = state_file_.string() + ".tmp";
        
        std::error_code ec;
        auto parent_path = std::filesystem::path(temp_file_).parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "SaveState: failed to create directory: " << ec.message();
                is_saving_ = false;
                return;
            }
        }
        
        GameState game_state;
        
        for (const auto& session : app_.GetSessions()) {
            std::unordered_map<authentication::Token, std::shared_ptr<Player>, 
                               authentication::TokenHasher> token_to_player;
            
            for (const auto& player : session->GetPlayers()) {
                auto token = app_.FindTokenByPlayer(player->GetId());
                if (token.has_value()) {
                    token_to_player[token.value()] = player;
                }
            }
            
            game_state.sessions.emplace_back(*session, token_to_player);
        }
        
        BOOST_LOG_TRIVIAL(info) << "SaveState: saving " << game_state.sessions.size() 
                               << " sessions to " << state_file_.string();
        
        std::ofstream ofs(temp_file_, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "SaveState: failed to open temporary file: " << temp_file_;
            is_saving_ = false;
            return;
        }
        
        {
            boost::archive::text_oarchive oa(ofs);
            oa << game_state;
        }
        
        ofs.flush();
        ofs.close();
        
        if (std::filesystem::file_size(temp_file_) == 0) {
            BOOST_LOG_TRIVIAL(error) << "SaveState: temporary file is empty";
            std::filesystem::remove(temp_file_);
            is_saving_ = false;
            return;
        }
        
        std::filesystem::rename(temp_file_, state_file_, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "SaveState: failed to rename file: " << ec.message();
            std::filesystem::remove(temp_file_, ec);
        } else {
            BOOST_LOG_TRIVIAL(info) << "SaveState: saved successfully to " << state_file_.string();
        }
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "SaveState: exception: " << e.what();
        if (!temp_file_.empty() && std::filesystem::exists(temp_file_)) {
            std::error_code ec;
            std::filesystem::remove(temp_file_, ec);
        }
    }
    
    is_saving_ = false;
}

bool StateSerializer::LoadState(net::io_context& ioc) {
    BOOST_LOG_TRIVIAL(info) << "LoadState: attempting to load from " << state_file_.string();
    
    try {
        if (!std::filesystem::exists(state_file_)) {
            BOOST_LOG_TRIVIAL(info) << "LoadState: file not found, starting with clean state";
            return false;
        }
        
        auto file_size = std::filesystem::file_size(state_file_);
        if (file_size == 0) {
            BOOST_LOG_TRIVIAL(warning) << "LoadState: file is empty, starting with clean state";
            return false;
        }
        
        BOOST_LOG_TRIVIAL(info) << "LoadState: loading from " << state_file_.string() 
                               << " (size=" << file_size << " bytes)";
        
        GameState game_state;
        std::ifstream ifs(state_file_.string());
        if (!ifs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "LoadState: failed to open file for reading";
            return false;
        }
        
        {
            boost::archive::text_iarchive ia(ifs);
            ia >> game_state;
        }
        ifs.close();
        
        BOOST_LOG_TRIVIAL(info) << "LoadState: loaded " << game_state.sessions.size() 
                               << " sessions from file";
        
        int restored_players_total = 0;
        
        for (const auto& session_ser : game_state.sessions) {
            auto map_id = session_ser.RestoreMapId();
            auto map = app_.FindMap(map_id);
            if (!map) {
                BOOST_LOG_TRIVIAL(error) << "LoadState: map not found for session: " << *map_id;
                continue;
            }
            
            auto session = std::make_shared<GameSession>(map, app_.GetTickPeriod(), 
                                                        app_.GetLootGeneratorConfig(), ioc);
            
            // Восстанавливаем потерянные объекты
            for (const auto& lost_obj_ser : session_ser.GetLostObjectsSerialize()) {
                session->AddLostObject(std::make_shared<model::LostObject>(lost_obj_ser.Restore()));
            }
            
            // Восстанавливаем игроков
            for (const auto& player_ser : session_ser.GetPlayersSerialize()) {
                auto player = std::make_shared<Player>(player_ser.Restore());
                auto dog = std::make_shared<model::Dog>(player_ser.RestoreDog());
                
                player->SetDog(dog);
                session->AddDog(dog);
                
                auto token = player_ser.RestoreToken();
                
                BOOST_LOG_TRIVIAL(debug) << "LoadState: restoring player " << player->GetName() 
                                        << " (id: " << *player->GetId() 
                                        << ", token: " << *token << ")";
                
                app_.RestorePlayer(token, player, session);
                restored_players_total++;
            }
            
            app_.AddGameSession(session);
            session->Run();
            
            BOOST_LOG_TRIVIAL(info) << "LoadState: session for map " << *map_id << " restored";
        }
        
        BOOST_LOG_TRIVIAL(info) << "LoadState: completed successfully - restored " 
                               << restored_players_total << " players";
        return true;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "LoadState: exception: " << e.what();
        return false;
    }
}

void StateSerializer::StartPeriodicSaving(net::io_context& ioc) {
    if (save_period_.count() <= 0) {
        BOOST_LOG_TRIVIAL(info) << "StartPeriodicSaving: periodic saving disabled (period=0)";
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "StartPeriodicSaving: starting with period " 
                           << save_period_.count() << "ms";
    
    auto strand = std::make_shared<net::strand<net::io_context::executor_type>>(net::make_strand(ioc));
    
    save_ticker_ = std::make_shared<time_m::Ticker>(
        strand,
        save_period_,
        [this](const std::chrono::milliseconds&) {
            this->SaveState();
        }
    );
    save_ticker_->Start();
    
    BOOST_LOG_TRIVIAL(info) << "StartPeriodicSaving: periodic saving started";
}

} // namespace app