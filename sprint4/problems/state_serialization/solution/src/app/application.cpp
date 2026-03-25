#include "application.h"
#include "logger.h"
#include "state_serializer.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/thread/future.hpp>
#include <iostream>
#include <filesystem>

namespace app {

using namespace std::literals;

void Application::SetSavingSettings(const saving::SavingSettings& settings) {
    saving_settings_ = settings;
    BOOST_LOG_TRIVIAL(info) << "Saving settings set: state_file=" 
                           << (saving_settings_.state_file_path.has_value() ? 
                               saving_settings_.state_file_path.value() : "none")
                           << ", period=" << (saving_settings_.period.has_value() ? 
                               std::to_string(saving_settings_.period.value().count()) : "none");
    
    if (saving_settings_.period.has_value() && saving_settings_.period.value().count() > 0) {
        save_period_counter_ = saving_settings_.period.value().count();
        BOOST_LOG_TRIVIAL(info) << "Save period counter initialized to " << save_period_counter_;
    }
}

bool Application::ShouldSaveState() const {
    bool has_period = saving_settings_.period.has_value() && saving_settings_.period.value().count() > 0;
    bool has_file = saving_settings_.state_file_path.has_value() && !saving_settings_.state_file_path.value().empty();
    
    BOOST_LOG_TRIVIAL(debug) << "ShouldSaveState: has_period=" << has_period 
                            << ", has_file=" << has_file;
    return has_period && has_file;
}

void Application::SetStateFilePath(const std::string& path) {
    state_file_path_ = path;
    BOOST_LOG_TRIVIAL(info) << "State file path set to: " << path;
}

std::string Application::GetStateFilePath() const {
    return state_file_path_;
}

void Application::SetSavePeriod(std::chrono::milliseconds period) {
    save_period_ = period;
    BOOST_LOG_TRIVIAL(info) << "Save period set to: " << period.count() << " ms";
}

bool Application::LoadGameStateFromFile() {
    if (state_file_path_.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "Cannot load state: state file path is empty";
        return false;
    }
    
    if (!std::filesystem::exists(state_file_path_)) {
        BOOST_LOG_TRIVIAL(info) << "State file does not exist: " << state_file_path_;
        return false;
    }
    
    try {
        BOOST_LOG_TRIVIAL(info) << "Loading game state from: " << state_file_path_;
        
        std::ifstream ifs(state_file_path_);
        if (!ifs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to open state file for reading";
            return false;
        }
        
        std::vector<game_data_ser::GameSessionSerialization> sessions_ser;
        {
            boost::archive::text_iarchive ia(ifs);
            ia >> sessions_ser;
        }
        ifs.close();
        
        // Восстанавливаем сессии
        for (const auto& session_ser : sessions_ser) {
            auto map_id = session_ser.RestoreMapId();
            auto map = game_.FindMap(map_id);
            if (!map) {
                BOOST_LOG_TRIVIAL(error) << "Map not found: " << *map_id;
                continue;
            }
            
            auto session = std::make_shared<GameSession>(map, tick_period_, game_.GetLootGeneratorConfig(), ioc_);
            
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
                RestorePlayer(token, player, session);
            }
            
            AddGameSession(session);
            session->Run();
        }
        
        BOOST_LOG_TRIVIAL(info) << "Game state loaded successfully";
        return true;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to load game state: " << e.what();
        return false;
    }
}

const model::Game::Maps& Application::ListMap() const noexcept {
    return game_.GetMaps();
};

const std::shared_ptr<model::Map> Application::FindMap(const model::Map::Id& id) const noexcept {
    return game_.FindMap(id);
};

std::tuple<authentication::Token, Player::Id> Application::JoinGame(
        const std::string& player_name,
        const model::Map::Id& id) {
    auto player = CreatePlayer(player_name);
    auto token = player_tokens_.AddPlayer(player);
    player_id_to_token_.emplace(Player::Id(*player->GetId()), token);
    std::shared_ptr<GameSession> game_session = FindGameSessionBy(id);
    if(!game_session){
        game_session = std::make_shared<GameSession>(game_.FindMap(id), tick_period_, game_.GetLootGeneratorConfig(), ioc_);
        AddGameSession(game_session);
        game_session->Run();
    }
    auth_token_to_session_index_[token] = game_session;
    BoundPlayerAndGameSession(player, game_session);
    game_session_to_token_player_pair_[game_session][token] = player;
    game_session->AddPlayer(player);
    
    BOOST_LOG_TRIVIAL(info) << "Player " << player_name << " joined with token " << *token 
                           << " and id " << *player->GetId();
    
    return std::tie(token, player->GetId());
};

std::shared_ptr<Player> Application::CreatePlayer(const std::string& player_name) {
    auto player = std::make_shared<Player>(player_name);
    players_.push_back(player);
    return player;
};

void Application::BoundPlayerAndGameSession(std::shared_ptr<Player> player,
                                    std::shared_ptr<GameSession> session){
    session_id_to_players_[session->GetId()].push_back(player);
    player->SetGameSession(session);
    auto dog = session->CreateDog(player->GetName(), *(session->GetMap()), randomize_spawn_points_);
    player->SetDog(dog);
};

const std::vector< std::shared_ptr<Player> >& Application::GetPlayersFromGameSession(const authentication::Token& token) {
    static const std::vector< std::shared_ptr<Player> > emptyPlayerList;
    auto player = player_tokens_.FindPlayerBy(token);
    if (!player) {
        return emptyPlayerList;
    }
    auto session_id = player->GetGameSessionId();
    if(!session_id_to_players_.contains(session_id)) {
        return emptyPlayerList;
    }
    return session_id_to_players_[session_id];
};

bool Application::IsExistPlayer(const authentication::Token& token) {
    return static_cast<bool>(player_tokens_.FindPlayerBy(token));
};

void Application::SetPlayerAction(const authentication::Token& token, model::Direction direction) {
    auto player = player_tokens_.FindPlayerBy(token);
    if (!player) return;
    auto dog = player->GetDog().lock();
    if (!dog) return;
    double velocity = player->GetGameSession()->GetMap()->GetDogVelocity();
    dog->SetAction(direction, velocity);
};

bool Application::IsManualTimeManagement() {
    return tick_period_.count() == 0;
};

void Application::UpdateGameState(const std::chrono::milliseconds& delta_time) {
    BOOST_LOG_TRIVIAL(debug) << "UpdateGameState called with delta=" << delta_time.count() << "ms";
    
    for(auto session : sessions_) {
        boost::promise<void> res_promise;
        auto res_future = res_promise.get_future();
        net::dispatch(*(session->GetStrand()),
            [session
            , &delta_time
            , &res_promise] {
                session->UpdateGameState(delta_time);
                res_promise.set_value();
            }
        );
        res_future.get();
    }
    SaveGameState(delta_time);
};

void Application::AddGameSession(std::shared_ptr<GameSession> session) {
    const size_t index = sessions_.size();
    if (auto [it, inserted] = map_id_to_session_index_.emplace(session->GetMap()->GetId(), index); !inserted) {
        throw std::invalid_argument("Game session with map id "s + *(session->GetMap()->GetId()) + " already exists"s);
    } else {
        try {
            sessions_.push_back(session);
            BOOST_LOG_TRIVIAL(info) << "Added game session for map " << *(session->GetMap()->GetId());
        } catch (...) {
            map_id_to_session_index_.erase(it);
            throw;
        }
    }
};

std::shared_ptr<GameSession> Application::FindGameSessionBy(const model::Map::Id& id) const noexcept {
    if (auto it = map_id_to_session_index_.find(id); it != map_id_to_session_index_.end()) {
        return sessions_.at(it->second);
    }
    return nullptr;
};

std::shared_ptr<GameSession> Application::FindGameSessionBy(const authentication::Token& token) const noexcept {
    if (auto it = auth_token_to_session_index_.find(token); it != auth_token_to_session_index_.end()) {
        return it->second;
    }
    return nullptr;
};

std::optional<authentication::Token> Application::FindTokenByPlayer(const Player::Id& player_id) const {
    auto it = player_id_to_token_.find(player_id);
    if (it != player_id_to_token_.end()) {
        return it->second;
    }
    return std::nullopt;
};

void Application::RestorePlayer(const authentication::Token& token, 
                                 std::shared_ptr<Player> player,
                                 std::shared_ptr<GameSession> session) {
    BOOST_LOG_TRIVIAL(info) << "Restoring player " << player->GetName() 
                            << " (id: " << *player->GetId() 
                            << ", token: " << *token << ")";
    
    player_tokens_.AddTokenPlayerPair(token, player);
    player_id_to_token_.emplace(Player::Id(*player->GetId()), token);
    player->SetGameSession(session);
    session_id_to_players_[session->GetId()].push_back(player);
    auth_token_to_session_index_[token] = session;
    game_session_to_token_player_pair_[session][token] = player;
    players_.push_back(player);
    session->AddPlayer(player);
    
    auto dog = player->GetDog().lock();
    if (dog) {
        session->AddDog(dog);
    }
    
    BOOST_LOG_TRIVIAL(info) << "Player " << player->GetName() << " restored successfully";
};

void Application::SaveGameState(const std::chrono::milliseconds& delta_time) {
    if (!ShouldSaveState()) {
        return;
    }
    
    if (save_period_counter_ <= 0) {
        save_period_counter_ = saving_settings_.period.value().count();
        return;
    }
    
    save_period_counter_ -= delta_time.count();
    
    if (save_period_counter_ <= 0) {
        BOOST_LOG_TRIVIAL(info) << "SaveGameState: TRIGGERING SAVE!";
        SaveGame();
        save_period_counter_ = saving_settings_.period.value().count();
    }
};

void Application::SaveGame() {
    using game_data_ser::GameSessionSerialization;
    
    if (!saving_settings_.state_file_path) {
        return;
    }
    
    static std::atomic<bool> is_saving{false};
    if (is_saving.exchange(true)) {
        BOOST_LOG_TRIVIAL(warning) << "Save already in progress, skipping";
        return;
    }
    
    std::string state_file = saving_settings_.state_file_path.value();
    std::string temp_file = state_file + ".tmp";
    
    BOOST_LOG_TRIVIAL(info) << "SaveGame: saving state to " << state_file;
    
    try {
        std::error_code ec;
        auto parent_path = std::filesystem::path(temp_file).parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Failed to create directory: " << ec.message();
                is_saving = false;
                return;
            }
        }
        
        std::vector<GameSessionSerialization> sessions_ser = GetSerializedData();
        
        std::ofstream ofs(temp_file, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to open temporary state file: " << temp_file;
            is_saving = false;
            return;
        }
        
        {
            boost::archive::text_oarchive oa(ofs);
            oa << sessions_ser;
        }
        
        ofs.flush();
        ofs.close();
        
        if (std::filesystem::file_size(temp_file) == 0) {
            BOOST_LOG_TRIVIAL(error) << "Temporary state file is empty";
            std::filesystem::remove(temp_file);
            is_saving = false;
            return;
        }
        
        std::filesystem::rename(temp_file, state_file, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to rename state file: " << ec.message();
            std::filesystem::remove(temp_file, ec);
        } else {
            BOOST_LOG_TRIVIAL(info) << "Game state saved successfully";
        }
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to save game state: " << e.what();
        if (!temp_file.empty() && std::filesystem::exists(temp_file)) {
            std::filesystem::remove(temp_file);
        }
    }
    
    is_saving = false;
};

std::vector<game_data_ser::GameSessionSerialization> Application::GetSerializedData() {
    using game_data_ser::GameSessionSerialization;
    std::vector<GameSessionSerialization> sessions_ser;
    sessions_ser.reserve(sessions_.size());
    
    for(auto session_ptr : sessions_) {
        boost::promise<GameSessionSerialization> promise;
        auto res_future = promise.get_future();
        net::dispatch(*(session_ptr->GetStrand()),
            [self = shared_from_this(), &promise, session_ptr] {
                std::unordered_map<authentication::Token, std::shared_ptr<app::Player>,
                                    authentication::TokenHasher> token_to_player;
                for (const auto& player : session_ptr->GetPlayers()) {
                    auto token = self->FindTokenByPlayer(player->GetId());
                    if (token.has_value()) {
                        token_to_player[token.value()] = player;
                    }
                }
                promise.set_value(
                    GameSessionSerialization(*session_ptr, token_to_player)
                );
            });
        sessions_ser.push_back(res_future.get());
    }
    return sessions_ser;
};

} // namespace app