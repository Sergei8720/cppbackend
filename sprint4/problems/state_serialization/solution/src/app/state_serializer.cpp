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
    // Даем время на запись
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
        
        // Создаем директорию, если её нет
        std::error_code ec;
        auto parent_path = std::filesystem::path(temp_file_).parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            BOOST_LOG_TRIVIAL(info) << "SaveState: creating directory " << parent_path.string();
            std::filesystem::create_directories(parent_path, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "SaveState: failed to create directory " << parent_path.string() 
                                        << " - " << ec.message();
                is_saving_ = false;
                return;
            }
            
            // Проверяем права после создания
            BOOST_LOG_TRIVIAL(info) << "SaveState: directory created, checking permissions";
            std::ofstream test_file(parent_path / "test.tmp");
            if (test_file.is_open()) {
                test_file << "test";
                test_file.close();
                std::filesystem::remove(parent_path / "test.tmp");
                BOOST_LOG_TRIVIAL(info) << "SaveState: directory is writable";
            } else {
                BOOST_LOG_TRIVIAL(error) << "SaveState: directory is NOT writable!";
            }
        }
        
        GameState game_state;
        
        int session_count = 0;
        for (const auto& session : app_.GetSessions()) {
            std::unordered_map<authentication::Token, std::shared_ptr<Player>, 
                               authentication::TokenHasher> token_to_player;
            
            int player_count = 0;
            for (const auto& player : session->GetPlayers()) {
                auto token = app_.FindTokenByPlayer(player->GetId());
                if (token.has_value()) {
                    token_to_player[token.value()] = player;
                    player_count++;
                    BOOST_LOG_TRIVIAL(debug) << "SaveState: saving player " << player->GetName() 
                                            << " with token " << *token.value();
                } else {
                    BOOST_LOG_TRIVIAL(warning) << "SaveState: player " << *player->GetId() 
                                               << " has no token, skipping";
                }
            }
            
            game_state.sessions.emplace_back(*session, token_to_player);
            session_count++;
            BOOST_LOG_TRIVIAL(info) << "SaveState: session " << session_count 
                                   << " has " << player_count << " players, "
                                   << token_to_player.size() << " tokens";
        }
        
        BOOST_LOG_TRIVIAL(info) << "SaveState: saving " << session_count 
                               << " sessions to " << state_file_.string();
        
        // Создаем временный файл
        std::ofstream ofs(temp_file_, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "SaveState: failed to open temporary file: " << temp_file_
                                    << " (errno: " << errno << ")";
            is_saving_ = false;
            return;
        }
        
        {
            boost::archive::text_oarchive oa(ofs);
            oa << game_state;
        }
        
        ofs.flush();
        ofs.close();
        
        // Проверяем, что файл записан
        if (!std::filesystem::exists(temp_file_)) {
            BOOST_LOG_TRIVIAL(error) << "SaveState: temporary file does not exist after write: " << temp_file_;
            is_saving_ = false;
            return;
        }
        
        auto file_size = std::filesystem::file_size(temp_file_);
        if (file_size == 0) {
            BOOST_LOG_TRIVIAL(error) << "SaveState: temporary file is empty: " << temp_file_;
            std::filesystem::remove(temp_file_);
            is_saving_ = false;
            return;
        }
        
        BOOST_LOG_TRIVIAL(info) << "SaveState: temporary file written, size=" << file_size;
        
        // Атомарное переименование
        std::filesystem::rename(temp_file_, state_file_, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "SaveState: failed to rename file: " << ec.message();
            std::filesystem::remove(temp_file_, ec);
        } else {
            BOOST_LOG_TRIVIAL(info) << "SaveState: saved successfully to " << state_file_.string()
                                   << " (size=" << std::filesystem::file_size(state_file_) << ")";
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
        int restored_lost_objects_total = 0;
        
        for (const auto& session_ser : game_state.sessions) {
            auto map_id = session_ser.RestoreMapId();
            auto map = app_.FindMap(map_id);
            if (!map) {
                BOOST_LOG_TRIVIAL(error) << "LoadState: map not found for session: " << *map_id;
                continue;
            }
            
            BOOST_LOG_TRIVIAL(debug) << "LoadState: creating session for map " << *map_id;
            auto session = std::make_shared<GameSession>(map, app_.GetTickPeriod(), 
                                                        app_.GetLootGeneratorConfig(), ioc);
            
            // Восстанавливаем потерянные объекты
            int lost_objects_restored = 0;
            for (const auto& lost_obj_ser : session_ser.GetLostObjectsSerialize()) {
                session->AddLostObject(std::make_shared<model::LostObject>(lost_obj_ser.Restore()));
                lost_objects_restored++;
            }
            restored_lost_objects_total += lost_objects_restored;
            BOOST_LOG_TRIVIAL(debug) << "LoadState: restored " << lost_objects_restored 
                                    << " lost objects for map " << *map_id;
            
            // Восстанавливаем игроков
            int players_restored = 0;
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
                players_restored++;
            }
            restored_players_total += players_restored;
            BOOST_LOG_TRIVIAL(debug) << "LoadState: restored " << players_restored 
                                    << " players for map " << *map_id;
            
            app_.AddGameSession(session);
            session->Run();
            
            BOOST_LOG_TRIVIAL(info) << "LoadState: session for map " << *map_id 
                                   << " restored and started with " << players_restored << " players";
        }
        
        BOOST_LOG_TRIVIAL(info) << "LoadState: completed successfully - restored " 
                               << restored_players_total << " players and "
                               << restored_lost_objects_total << " lost objects";
        return true;
        
    } catch (const boost::archive::archive_exception& e) {
        BOOST_LOG_TRIVIAL(error) << "LoadState: archive error: " << e.what();
        return false;
    } catch (const std::ifstream::failure& e) {
        BOOST_LOG_TRIVIAL(error) << "LoadState: file I/O error: " << e.what();
        return false;
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