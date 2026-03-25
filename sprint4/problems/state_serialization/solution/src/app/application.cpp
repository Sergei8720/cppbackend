#include "application.h"
#include "logger.h"

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
    
    // Инициализируем счетчик, если период задан
    if (saving_settings_.period.has_value() && saving_settings_.period.value().count() > 0) {
        save_period_counter_ = saving_settings_.period.value().count();
        BOOST_LOG_TRIVIAL(debug) << "Save period counter initialized to " << save_period_counter_;
    }
}

bool Application::ShouldSaveState() const {
    bool result = saving_settings_.period.has_value() && 
                  saving_settings_.period.value().count() > 0 &&
                  saving_settings_.state_file_path.has_value() &&
                  !saving_settings_.state_file_path.value().empty();
    
    BOOST_LOG_TRIVIAL(debug) << "ShouldSaveState: " << result 
                             << " (period exists=" << saving_settings_.period.has_value()
                             << ", period value=" << (saving_settings_.period.has_value() ? 
                                 std::to_string(saving_settings_.period.value().count()) : "0")
                             << ", state_file exists=" << saving_settings_.state_file_path.has_value() << ")";
    return result;
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
    bool exists = static_cast<bool>(player_tokens_.FindPlayerBy(token));
    BOOST_LOG_TRIVIAL(debug) << "IsExistPlayer for token " << *token << ": " << exists;
    return exists;
};

void Application::SetPlayerAction(const authentication::Token& token, model::Direction direction) {
    auto player = player_tokens_.FindPlayerBy(token);
    if (!player) return;
    auto dog = player->GetDog().lock();
    if (!dog) return;
    double velocity = player->GetGameSession()->GetMap()->GetDogVelocity();
    dog->SetAction(direction, velocity);
    BOOST_LOG_TRIVIAL(debug) << "Set player action: token=" << *token 
                             << ", direction=" << static_cast<int>(direction);
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
    
    // 1. Добавляем связь token -> player в PlayerTokens
    player_tokens_.AddTokenPlayerPair(token, player);
    BOOST_LOG_TRIVIAL(debug) << "Added token->player mapping";
    
    // 2. Добавляем связь player_id -> token
    player_id_to_token_.emplace(Player::Id(*player->GetId()), token);
    BOOST_LOG_TRIVIAL(debug) << "Added player_id->token mapping";
    
    // 3. Устанавливаем сессию игроку
    player->SetGameSession(session);
    BOOST_LOG_TRIVIAL(debug) << "Set player session";
    
    // 4. Добавляем игрока в список игроков сессии
    session_id_to_players_[session->GetId()].push_back(player);
    BOOST_LOG_TRIVIAL(debug) << "Added player to session players list";
    
    // 5. Добавляем связь token -> session
    auth_token_to_session_index_[token] = session;
    BOOST_LOG_TRIVIAL(debug) << "Added token->session mapping";
    
    // 6. Добавляем связь session -> token -> player
    game_session_to_token_player_pair_[session][token] = player;
    BOOST_LOG_TRIVIAL(debug) << "Added session->token->player mapping";
    
    // 7. Добавляем игрока в общий список
    players_.push_back(player);
    BOOST_LOG_TRIVIAL(debug) << "Added player to global players list";
    
    // 8. Добавляем игрока в сессию
    session->AddPlayer(player);
    BOOST_LOG_TRIVIAL(debug) << "Added player to session";
    
    // 9. Восстанавливаем собаку
    auto dog = player->GetDog().lock();
    if (dog) {
        session->AddDog(dog);
        BOOST_LOG_TRIVIAL(debug) << "Added dog to session";
    }
    
    BOOST_LOG_TRIVIAL(info) << "Player " << player->GetName() << " restored successfully";
};

void Application::SaveGameState(const std::chrono::milliseconds& delta_time) {
    BOOST_LOG_TRIVIAL(debug) << "SaveGameState called: delta=" << delta_time.count() 
                             << "ms, counter=" << save_period_counter_
                             << ", period=" << (saving_settings_.period.has_value() ? 
                                 std::to_string(saving_settings_.period.value().count()) : "none");
    
    if (!ShouldSaveState()) {
        BOOST_LOG_TRIVIAL(debug) << "SaveGameState: ShouldSaveState returned false, skipping";
        return;
    }
    
    // Если счетчик еще не инициализирован (хотя должен быть из SetSavingSettings)
    if (save_period_counter_ <= 0) {
        save_period_counter_ = saving_settings_.period.value().count();
        BOOST_LOG_TRIVIAL(warning) << "SaveGameState: counter was <=0, reinitialized to " << save_period_counter_;
        return;
    }
    
    save_period_counter_ -= delta_time.count();
    BOOST_LOG_TRIVIAL(debug) << "SaveGameState: counter after subtract = " << save_period_counter_;
    
    if (save_period_counter_ <= 0) {
        BOOST_LOG_TRIVIAL(info) << "SaveGameState: TRIGGERING SAVE!";
        SaveGame();
        save_period_counter_ = saving_settings_.period.value().count();
        BOOST_LOG_TRIVIAL(debug) << "SaveGameState: counter reset to " << save_period_counter_;
    }
};

void Application::SaveGame() {
    using game_data_ser::GameSessionSerialization;
    
    if (!saving_settings_.state_file_path) {
        BOOST_LOG_TRIVIAL(warning) << "SaveGame: state_file_path is empty, skipping";
        return;
    }
    
    static std::atomic<bool> is_saving{false};
    if (is_saving.exchange(true)) {
        BOOST_LOG_TRIVIAL(warning) << "SaveGame: already in progress, skipping";
        return;
    }
    
    std::string state_file = saving_settings_.state_file_path.value();
    std::string temp_file = state_file + ".tmp";
    
    BOOST_LOG_TRIVIAL(info) << "SaveGame: saving state to " << state_file;
    
    try {
        // Создаем директорию, если её нет
        std::error_code ec;
        auto parent_path = std::filesystem::path(temp_file).parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            BOOST_LOG_TRIVIAL(debug) << "SaveGame: creating directory " << parent_path.string();
            std::filesystem::create_directories(parent_path, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "SaveGame: failed to create directory " << parent_path.string() 
                                        << " - " << ec.message();
                is_saving = false;
                return;
            }
        }
        
        std::vector<GameSessionSerialization> sessions_ser = GetSerializedData();
        BOOST_LOG_TRIVIAL(debug) << "SaveGame: serialized " << sessions_ser.size() << " sessions";
        
        std::ofstream ofs(temp_file, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "SaveGame: failed to open temporary file " << temp_file;
            is_saving = false;
            return;
        }
        
        {
            boost::archive::text_oarchive oa(ofs);
            oa << sessions_ser;
        }
        
        ofs.flush();
        ofs.close();
        
        // Проверяем, что файл записан
        if (std::filesystem::file_size(temp_file) == 0) {
            BOOST_LOG_TRIVIAL(error) << "SaveGame: temporary file is empty: " << temp_file;
            std::filesystem::remove(temp_file);
            is_saving = false;
            return;
        }
        
        BOOST_LOG_TRIVIAL(debug) << "SaveGame: temporary file written, size=" 
                                 << std::filesystem::file_size(temp_file);
        
        // Атомарное переименование
        std::filesystem::rename(temp_file, state_file, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "SaveGame: failed to rename file: " << ec.message();
            std::filesystem::remove(temp_file, ec);
        } else {
            BOOST_LOG_TRIVIAL(info) << "SaveGame: state saved successfully to " << state_file;
        }
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "SaveGame: exception: " << e.what();
        if (!temp_file.empty() && std::filesystem::exists(temp_file)) {
            std::error_code ec;
            std::filesystem::remove(temp_file, ec);
        }
    }
    
    is_saving = false;
};

std::vector<game_data_ser::GameSessionSerialization> Application::GetSerializedData() {
    using game_data_ser::GameSessionSerialization;
    std::vector<GameSessionSerialization> sessions_ser;
    sessions_ser.reserve(sessions_.size());
    
    BOOST_LOG_TRIVIAL(debug) << "GetSerializedData: serializing " << sessions_.size() << " sessions";
    
    for(auto session_ptr : sessions_) {
        boost::promise<GameSessionSerialization> promise;
        auto res_future = promise.get_future();
        net::dispatch(*(session_ptr->GetStrand()),
            [self = shared_from_this(), &promise, session_ptr] {
                std::unordered_map<authentication::Token, std::shared_ptr<app::Player>,
                                    authentication::TokenHasher> token_to_player;
                
                int player_count = 0;
                for (const auto& player : session_ptr->GetPlayers()) {
                    auto token = self->FindTokenByPlayer(player->GetId());
                    if (token.has_value()) {
                        token_to_player[token.value()] = player;
                        player_count++;
                        BOOST_LOG_TRIVIAL(debug) << "GetSerializedData: player " << player->GetName() 
                                                << " (id=" << *player->GetId() 
                                                << ") with token " << *token.value();
                    } else {
                        BOOST_LOG_TRIVIAL(warning) << "GetSerializedData: player " << *player->GetId() 
                                                   << " has no token, skipping";
                    }
                }
                BOOST_LOG_TRIVIAL(debug) << "GetSerializedData: serialized " << player_count << " players for session";
                promise.set_value(GameSessionSerialization(*session_ptr, token_to_player));
            });
        sessions_ser.push_back(res_future.get());
    }
    
    BOOST_LOG_TRIVIAL(debug) << "GetSerializedData: completed, " << sessions_ser.size() << " sessions serialized";
    return sessions_ser;
};

} // namespace app