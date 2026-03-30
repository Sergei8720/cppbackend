#include "application.h"
#include "logger.h"
#include "state_serializer.h"
#include "database/database.h"

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
        game_session = std::make_shared<GameSession>(
            game_.FindMap(id), 
            tick_period_, 
            game_.GetLootGeneratorConfig(), 
            ioc_,
            game_.GetDogRetirementTime()  // Передаем время бездействия
        );
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
    auto player = player_tokens_.FindPlayerBy(token);
    bool exists = static_cast<bool>(player);
    BOOST_LOG_TRIVIAL(debug) << "IsExistPlayer: token=" << *token 
                             << " exists=" << exists;
    if (exists && player) {
        BOOST_LOG_TRIVIAL(debug) << "  Player name=" << player->GetName() 
                                 << " id=" << *player->GetId()
                                 << " session_id=" << *(player->GetGameSessionId());
    }
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
    BOOST_LOG_TRIVIAL(debug) << "UpdateGameState called with delta_time=" << delta_time.count() << "ms";
    
    for(auto session : sessions_) {
        boost::promise<void> res_promise;
        auto res_future = res_promise.get_future();
        net::dispatch(*(session->GetStrand()),
            [session, &delta_time, &res_promise] {
                session->UpdateGameState(delta_time);
                res_promise.set_value();
            }
        );
        res_future.get();
    }
    
    // Периодическое сохранение
    if (saving_settings_.period.has_value() && saving_settings_.period.value().count() > 0) {
        static std::chrono::milliseconds elapsed_since_last_save{0};
        elapsed_since_last_save += delta_time;
        
        BOOST_LOG_TRIVIAL(debug) << "Save check: elapsed=" << elapsed_since_last_save.count() 
                                 << "ms, period=" << saving_settings_.period.value().count() << "ms";
        
        if (elapsed_since_last_save >= saving_settings_.period.value()) {
            BOOST_LOG_TRIVIAL(info) << "Periodic save triggered! elapsed=" 
                                    << elapsed_since_last_save.count() << "ms";
            SaveGame();
            elapsed_since_last_save = std::chrono::milliseconds{0};
        }
    }
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
    BOOST_LOG_TRIVIAL(info) << "  Session: " << *(session->GetId());
    
    auto existing_player = player_tokens_.FindPlayerBy(token);
    if (existing_player) {
        BOOST_LOG_TRIVIAL(warning) << "  Token " << *token << " already exists for player " 
                                   << existing_player->GetName() << " id=" << *existing_player->GetId();
    }
    
    player_tokens_.AddTokenPlayerPair(token, player);
    BOOST_LOG_TRIVIAL(info) << "  Added token->player mapping";
    
    player_id_to_token_.emplace(Player::Id(*player->GetId()), token);
    BOOST_LOG_TRIVIAL(info) << "  Added player_id->token mapping";
    
    player->SetGameSession(session);
    BOOST_LOG_TRIVIAL(info) << "  Set game session for player";
    
    session_id_to_players_[session->GetId()].push_back(player);
    BOOST_LOG_TRIVIAL(info) << "  Added to session players list";
    
    auth_token_to_session_index_[token] = session;
    BOOST_LOG_TRIVIAL(info) << "  Added token->session mapping";
    
    game_session_to_token_player_pair_[session][token] = player;
    BOOST_LOG_TRIVIAL(info) << "  Added session->token->player mapping";
    
    players_.push_back(player);
    BOOST_LOG_TRIVIAL(info) << "  Added to global players list";
    
    session->AddPlayer(player);
    BOOST_LOG_TRIVIAL(info) << "  Added to session";
    
    auto dog = player->GetDog().lock();
    if (dog) {
        session->AddDog(dog);
        BOOST_LOG_TRIVIAL(info) << "  Added dog to session, dog_id=" << *dog->GetId();
    } else {
        BOOST_LOG_TRIVIAL(warning) << "  No dog found for player!";
    }
    
    BOOST_LOG_TRIVIAL(info) << "=== RestorePlayer completed ===";
};

void Application::SaveGame() {
    BOOST_LOG_TRIVIAL(info) << "Application::SaveGame() called";
    
    if (!saving_settings_.state_file_path) {
        BOOST_LOG_TRIVIAL(warning) << "SaveGame: no state file path set";
        return;
    }
    
    if (state_serializer_) {
        BOOST_LOG_TRIVIAL(info) << "Calling StateSerializer::SaveState()";
        state_serializer_->SaveState();
    } else {
        BOOST_LOG_TRIVIAL(warning) << "SaveGame: state_serializer_ is null!";
    }
};

void Application::SaveGameState(const std::chrono::milliseconds& delta_time) {
    return;
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

// Метод для удаления игрока и сохранения рекорда
void Application::RemovePlayerAndSaveRecord(const authentication::Token& token, std::shared_ptr<Player> player) {
    BOOST_LOG_TRIVIAL(info) << "Removing player " << player->GetName() << " with token " << *token;
    
    // Получаем собаку
    auto dog = player->GetDog().lock();
    if (!dog) {
        BOOST_LOG_TRIVIAL(warning) << "Player " << player->GetName() << " has no dog";
        return;
    }
    
    // Вычисляем время игры (от момента присоединения до текущего момента)
    // В реальном приложении нужно отслеживать время входа, здесь используем упрощённый подход
    auto now = std::chrono::steady_clock::now();
    // TODO: нужно хранить время входа игрока
    
    // Сохраняем рекорд в БД, если есть подключение
    if (db_pool_) {
        try {
            // Генерируем UUID для записи (используем dog_id как строку)
            std::string uuid = std::to_string(*dog->GetId());
            
            database::PlayerRecord record{
                uuid,
                player->GetName(),
                static_cast<int64_t>(dog->GetScore()),
                0  // TODO: нужно передать реальное время игры
            };
            
            database::Database::SaveRecord(db_pool_, record);
            BOOST_LOG_TRIVIAL(info) << "Saved retirement record for " << player->GetName() 
                                    << " with score " << dog->GetScore();
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Failed to save retirement record: " << e.what();
        }
    } else {
        BOOST_LOG_TRIVIAL(debug) << "No database connection, skipping record save";
    }
    
    // Удаляем из всех структур
    auto session = player->GetGameSession();
    if (session) {
        auto dog_id = dog->GetId();
        // Удаляем собаку из сессии
        // session->RemoveDog(dog_id); // TODO: добавить метод в GameSession
    }
    
    // Удаляем токен
    player_tokens_.RemoveToken(token); // TODO: добавить метод в PlayerTokens
    
    // Удаляем из player_id_to_token_
    player_id_to_token_.erase(player->GetId());
    
    // Удаляем из session_id_to_players_
    auto session_id = player->GetGameSessionId();
    if (session_id_to_players_.contains(session_id)) {
        auto& players = session_id_to_players_[session_id];
        std::erase(players, player);
    }
    
    // Удаляем из auth_token_to_session_index_
    auth_token_to_session_index_.erase(token);
    
    // Удаляем из game_session_to_token_player_pair_
    if (session) {
        if (game_session_to_token_player_pair_.contains(session)) {
            game_session_to_token_player_pair_[session].erase(token);
        }
    }
    
    // Удаляем из players_
    std::erase(players_, player);
    
    BOOST_LOG_TRIVIAL(info) << "Player " << player->GetName() << " removed successfully";
};

} // namespace app