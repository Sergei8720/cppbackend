#include "application.h"
#include "logger.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/thread/future.hpp>
#include <iostream>
#include <filesystem>

namespace app {

using namespace std::literals;

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
    BOOST_LOG_TRIVIAL(debug) << "IsExistPlayer: token=" << *token << " exists=" << exists;
    return exists;
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
    // Сохранение теперь только через StateSerializer
};

void Application::AddGameSession(std::shared_ptr<GameSession> session) {
    const size_t index = sessions_.size();
    if (auto [it, inserted] = map_id_to_session_index_.emplace(session->GetMap()->GetId(), index); !inserted) {
        throw std::invalid_argument("Game session with map id "s + *(session->GetMap()->GetId()) + " already exists"s);
    } else {
        try {
            sessions_.push_back(session);
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
    BOOST_LOG_TRIVIAL(info) << "=== RestorePlayer called ===";
    BOOST_LOG_TRIVIAL(info) << "  Player: " << player->GetName() << " id=" << *player->GetId();
    BOOST_LOG_TRIVIAL(info) << "  Token: " << *token;
    BOOST_LOG_TRIVIAL(info) << "  Session: " << *session->GetId();
    
    // 1. Добавляем связь token -> player в PlayerTokens
    player_tokens_.AddTokenPlayerPair(token, player);
    BOOST_LOG_TRIVIAL(info) << "  Added token->player mapping";
    
    // 2. Добавляем связь player_id -> token
    player_id_to_token_.emplace(Player::Id(*player->GetId()), token);
    BOOST_LOG_TRIVIAL(info) << "  Added player_id->token mapping";
    
    // 3. Устанавливаем сессию игроку
    player->SetGameSession(session);
    BOOST_LOG_TRIVIAL(info) << "  Set game session for player";
    
    // 4. Добавляем игрока в список игроков сессии
    session_id_to_players_[session->GetId()].push_back(player);
    BOOST_LOG_TRIVIAL(info) << "  Added to session players list";
    
    // 5. Добавляем связь token -> session
    auth_token_to_session_index_[token] = session;
    BOOST_LOG_TRIVIAL(info) << "  Added token->session mapping";
    
    // 6. Добавляем связь session -> token -> player
    game_session_to_token_player_pair_[session][token] = player;
    BOOST_LOG_TRIVIAL(info) << "  Added session->token->player mapping";
    
    // 7. Добавляем игрока в общий список
    players_.push_back(player);
    BOOST_LOG_TRIVIAL(info) << "  Added to global players list";
    
    // 8. Добавляем игрока в сессию
    session->AddPlayer(player);
    BOOST_LOG_TRIVIAL(info) << "  Added to session";
    
    // 9. Восстанавливаем собаку
    auto dog = player->GetDog().lock();
    if (dog) {
        session->AddDog(dog);
        BOOST_LOG_TRIVIAL(info) << "  Added dog to session";
    }
    
    BOOST_LOG_TRIVIAL(info) << "=== RestorePlayer completed ===";
};

void Application::SaveGameState(const std::chrono::milliseconds& delta_time) {
    // ⚠️ УДАЛЕНО: сохранение теперь только через StateSerializer
    // Эта функция оставлена пустой, чтобы не нарушать существующие вызовы
    return;
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
    
    std::string temp_file = saving_settings_.state_file_path.value() + ".tmp";
    
    try {
        // Создаем директорию, если её нет
        std::error_code ec;
        auto parent_path = std::filesystem::path(temp_file).parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Failed to create directory: " << parent_path.string() 
                                        << " - " << ec.message();
                is_saving = false;
                return;
            }
            BOOST_LOG_TRIVIAL(debug) << "Created directory: " << parent_path.string();
        }
        
        BOOST_LOG_TRIVIAL(info) << "Saving game state to " << saving_settings_.state_file_path.value();
        
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
        
        // Проверяем, что файл записан
        if (std::filesystem::file_size(temp_file) == 0) {
            BOOST_LOG_TRIVIAL(error) << "Temporary state file is empty: " << temp_file;
            std::filesystem::remove(temp_file);
            is_saving = false;
            return;
        }
        
        // Атомарное переименование
        std::filesystem::rename(temp_file, saving_settings_.state_file_path.value(), ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to rename state file: " << ec.message();
            std::filesystem::remove(temp_file, ec);
        } else {
            BOOST_LOG_TRIVIAL(info) << "Game state saved successfully to " 
                                   << saving_settings_.state_file_path.value();
        }
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to save game state: " << e.what();
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
                        BOOST_LOG_TRIVIAL(debug) << "Serializing player " << player->GetName() 
                                                << " with token " << *token.value();
                    } else {
                        BOOST_LOG_TRIVIAL(warning) << "Player " << *player->GetId() 
                                                   << " has no token, skipping from serialization";
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