#include "application.h"
#include <boost/log/trivial.hpp>

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
    
    // ИСПРАВЛЕНО: используем emplace вместо operator[]
    player_id_to_token_.emplace(player->GetId(), token);
    
    std::shared_ptr<GameSession> game_session = FindGameSessionBy(id);
    if(!game_session){
        game_session = std::make_shared<GameSession>(game_.FindMap(id), tick_period_, game_.GetLootGeneratorConfig(), ioc_);
        AddGameSession(game_session);
        game_session->Run();
    }
    auth_token_to_session_index_.emplace(token, game_session);  // ИСПРАВЛЕНО
    BoundPlayerAndGameSession(player, game_session);
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
    if (!player) {
        return;
    }
    auto dog = player->GetDog().lock();
    if (!dog) {
        return;
    }
    double velocity = player->GetGameSession()->GetMap()->GetDogVelocity();
    dog->SetAction(direction, velocity);
};

bool Application::IsManualTimeManagement() {
    return tick_period_.count() == 0;
};

void Application::UpdateGameState(const std::chrono::milliseconds& delta_time) {
    for(auto session : sessions_) {
        session->UpdateGameState(delta_time);
    }
};

void Application::AddGameSession(std::shared_ptr<GameSession> session) {
    const size_t index = sessions_.size();
    
    auto it = map_id_to_session_index_.find(session->GetMap()->GetId());
    if (it != map_id_to_session_index_.end()) {
        throw std::invalid_argument("Game session with map id "s + *(session->GetMap()->GetId()) + " already exists"s);
    }
    
    it = map_id_to_session_index_.emplace(session->GetMap()->GetId(), index).first;

    try {
        sessions_.push_back(session);
    } catch (const std::exception& e) {
        map_id_to_session_index_.erase(it);
        BOOST_LOG_TRIVIAL(error) << "Failed to add game session: " << e.what();
        throw;
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

const std::vector< std::shared_ptr<GameSession> >& Application::GetSessions() {
    return sessions_;
};

// МЕТОДЫ ДЛЯ СЕРИАЛИЗАЦИИ

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
    // Восстанавливаем игрока
    players_.push_back(player);
    
    // Восстанавливаем токен
    player_tokens_.RestoreToken(token, player);
    
    // Восстанавливаем связи - ИСПРАВЛЕНО: используем emplace
    player_id_to_token_.emplace(player->GetId(), token);
    auth_token_to_session_index_.emplace(token, session);
    session_id_to_players_[session->GetId()].push_back(player);
    
    // Восстанавливаем связь с сессией
    player->SetGameSession(session);
};

}