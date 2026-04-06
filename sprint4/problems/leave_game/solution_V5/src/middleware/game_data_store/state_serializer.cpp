#include "state_serializer.h"
#include "logger.h"
#include "dog.h"
#include "lost_object.h"
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
}

void StateSerializer::FinalSave() {
    if (is_final_save_done_.exchange(true)) {
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "Performing final save...";
    SaveState();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void StateSerializer::SaveState() {
    if (state_file_.empty()) return;
    
    bool expected = false;
    if (!is_saving_.compare_exchange_strong(expected, true)) {
        BOOST_LOG_TRIVIAL(warning) << "Save already in progress, skipping";
        return;
    }
    
    try {
        temp_file_ = state_file_.string() + ".tmp";
        
        std::error_code ec;
        auto parent_path = std::filesystem::path(temp_file_).parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Failed to create directory: " << parent_path.string() << " - " << ec.message();
                is_saving_ = false;
                return;
            }
            BOOST_LOG_TRIVIAL(debug) << "Created directory: " << parent_path.string();
        }
        
        GameState game_state;
        
        game_state.max_player_id = static_cast<uint64_t>(Player::GetMaxId());
        game_state.max_dog_id = static_cast<uint64_t>(model::Dog::GetMaxId());
        game_state.max_loot_id = static_cast<uint64_t>(model::LostObject::GetMaxId());
        
        BOOST_LOG_TRIVIAL(info) << "=== SAVING GAME STATE ===";
        BOOST_LOG_TRIVIAL(info) << "Saving counters: player_max=" << game_state.max_player_id
                                 << ", dog_max=" << game_state.max_dog_id
                                 << ", loot_max=" << game_state.max_loot_id;
        
        for (const auto& session : app_.GetSessions()) {
            std::unordered_map<authentication::Token, std::shared_ptr<Player>, 
                               authentication::TokenHasher> token_to_player;
            
            BOOST_LOG_TRIVIAL(info) << "Session for map " << *session->GetMap()->GetId() 
                                    << " has " << session->GetPlayers().size() << " players";
            
            for (const auto& player : session->GetPlayers()) {
                auto token = app_.FindTokenByPlayer(*player->GetId());
                if (token.has_value()) {
                    token_to_player[token.value()] = player;
                    BOOST_LOG_TRIVIAL(info) << "  Saving player: " << player->GetName() 
                                            << " id=" << *player->GetId() 
                                            << " token=" << *token.value()
                                            << " join_time_ns=" << player->GetJoinTime().time_since_epoch().count();
                } else {
                    BOOST_LOG_TRIVIAL(warning) << "  Player " << player->GetName() 
                                               << " id=" << *player->GetId() 
                                               << " has NO token!";
                }
            }
            
            game_state.data.sessions.emplace_back(*session, token_to_player);
        }
        
        BOOST_LOG_TRIVIAL(info) << "Total sessions saved: " << game_state.data.sessions.size();
        
        std::ofstream ofs(temp_file_, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to open temporary state file: " << temp_file_;
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
            BOOST_LOG_TRIVIAL(error) << "Temporary state file is empty: " << temp_file_;
            std::filesystem::remove(temp_file_);
            is_saving_ = false;
            return;
        }
        
        std::filesystem::rename(temp_file_, state_file_, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to rename state file: " << ec.message();
            std::filesystem::remove(temp_file_, ec);
        } else {
            BOOST_LOG_TRIVIAL(info) << "Game state saved to " << state_file_.string();
        }
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to save game state: " << e.what();
        if (!temp_file_.empty() && std::filesystem::exists(temp_file_)) {
            std::error_code ec;
            std::filesystem::remove(temp_file_, ec);
        }
    }
    
    is_saving_ = false;
}

bool StateSerializer::LoadState(net::io_context& ioc) {
    try {
        if (!std::filesystem::exists(state_file_)) {
            BOOST_LOG_TRIVIAL(info) << "State file not found: " << state_file_.string();
            return false;
        }
        
        if (std::filesystem::file_size(state_file_) == 0) {
            BOOST_LOG_TRIVIAL(warning) << "State file is empty: " << state_file_.string();
            return false;
        }
        
        BOOST_LOG_TRIVIAL(info) << "=== LOADING GAME STATE ===";
        BOOST_LOG_TRIVIAL(info) << "Loading game state from " << state_file_.string();
        
        GameState game_state;
        std::ifstream ifs(state_file_.string());
        if (!ifs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to open state file for reading: " << state_file_.string();
            return false;
        }
        
        {
            boost::archive::text_iarchive ia(ifs);
            ia >> game_state;
        }
        ifs.close();
        
        BOOST_LOG_TRIVIAL(info) << "Loaded " << game_state.data.sessions.size() 
                               << " sessions from state file";
        
        size_t max_player_id_restored = static_cast<size_t>(game_state.max_player_id);
        size_t max_dog_id_restored = static_cast<size_t>(game_state.max_dog_id);
        size_t max_loot_id_restored = static_cast<size_t>(game_state.max_loot_id);
        
        Player::ResetMaxId(max_player_id_restored);
        model::Dog::ResetMaxId(max_dog_id_restored);
        model::LostObject::ResetMaxId(max_loot_id_restored);
        
        BOOST_LOG_TRIVIAL(info) << "Restored counters: player_max=" << max_player_id_restored
                                << ", dog_max=" << max_dog_id_restored
                                << ", loot_max=" << max_loot_id_restored;
        
        for (const auto& session_ser : game_state.data.sessions) {
            auto map_id = session_ser.RestoreMapId();
            BOOST_LOG_TRIVIAL(info) << "Restoring session for map: " << *map_id;
            
            auto map = app_.FindMap(map_id);
            if (!map) {
                BOOST_LOG_TRIVIAL(error) << "Map not found for restored session: " << *map_id;
                continue;
            }
            
            auto session = std::make_shared<GameSession>(
                map, 
                app_.GetTickPeriod(), 
                app_.GetLootGeneratorConfig(), 
                ioc,
                app_.GetDogRetirementTime()
            );
            
            session->SetRetirementCallback(
                [this](const authentication::Token& token, size_t player_id, int64_t play_time_ms) {
                    BOOST_LOG_TRIVIAL(info) << "LoadState: retirement callback triggered for player_id=" << player_id;
                    auto player = app_.FindPlayerById(player_id);
                    if (player) {
                        app_.RemovePlayerAndSaveRecord(token, player, play_time_ms);
                    }
                }
            );
            
            session->SetTokenFinder(
                [this](size_t player_id) -> std::optional<authentication::Token> {
                    return app_.FindTokenByPlayer(player_id);
                }
            );
            
            size_t lost_objects_restored = 0;
            for (const auto& lost_obj_ser : session_ser.GetLostObjectsSerialize()) {
                auto lost_obj = std::make_shared<model::LostObject>(lost_obj_ser.Restore());
                session->AddLostObject(lost_obj);
                lost_objects_restored++;
                if (*lost_obj->GetId() >= model::LostObject::GetMaxId()) {
                    model::LostObject::ResetMaxId(*lost_obj->GetId() + 1);
                }
            }
            BOOST_LOG_TRIVIAL(info) << "  Restored " << lost_objects_restored << " lost objects";
            
            const auto& players_ser = session_ser.GetPlayersSerialize();
            BOOST_LOG_TRIVIAL(info) << "  Found " << players_ser.size() << " players to restore";
            
            for (const auto& player_ser : players_ser) {
                auto player = std::make_shared<Player>(player_ser.Restore());
                player->SetJoinTime(std::chrono::steady_clock::time_point(
                    std::chrono::nanoseconds(player_ser.GetJoinTimeNs())
                ));
                
                auto dog = std::make_shared<model::Dog>(player_ser.RestoreDog());
                
                player->SetDog(dog);
                session->AddDog(dog);
                
                BOOST_LOG_TRIVIAL(debug) << "    Restored join_time_ns: " << player_ser.GetJoinTimeNs();
                
                if (*player->GetId() >= Player::GetMaxId()) {
                    Player::ResetMaxId(*player->GetId() + 1);
                }
                if (*dog->GetId() >= model::Dog::GetMaxId()) {
                    model::Dog::ResetMaxId(*dog->GetId() + 1);
                }
                
                auto token = player_ser.RestoreToken();
                
                BOOST_LOG_TRIVIAL(info) << "    Restoring player: " << player->GetName() 
                                        << " id=" << *player->GetId() 
                                        << " token=" << *token;
                
                app_.RestorePlayer(token, player, session);
            }
            
            app_.AddGameSession(session);
            session->Run();
            
            BOOST_LOG_TRIVIAL(info) << "  Session for map " << *map_id << " restored and started";
        }
        
        BOOST_LOG_TRIVIAL(info) << "Game state loaded successfully from " << state_file_.string();
        return true;
        
    } catch (const boost::archive::archive_exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Archive error while loading game state: " << e.what();
        return false;
    } catch (const std::ifstream::failure& e) {
        BOOST_LOG_TRIVIAL(error) << "File I/O error while loading game state: " << e.what();
        return false;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to load game state: " << e.what();
        return false;
    }
}

void StateSerializer::StartPeriodicSaving(net::io_context& ioc) {
    if (save_period_.count() <= 0) {
        BOOST_LOG_TRIVIAL(info) << "Periodic saving disabled (period = 0)";
        return;
    }
    
    auto strand = std::make_shared<net::strand<net::io_context::executor_type>>(net::make_strand(ioc));
    
    save_ticker_ = std::make_shared<time_m::Ticker>(
        strand,
        save_period_,
        [this](const std::chrono::milliseconds&) {
            this->SaveState();
        }
    );
    save_ticker_->Start();
    
    BOOST_LOG_TRIVIAL(info) << "Periodic saving started with period " << save_period_.count() << " ms";
}

}