#include "game_session_manager.h"

namespace app {

GameSessionManager::GameSessionManager(net::io_context& ioc)
    : ioc_(ioc) {
}

std::shared_ptr<GameSession> GameSessionManager::FindOrCreateSession(std::shared_ptr<model::Map> map) {
    auto map_id = map->GetId();
    
    auto it = map_to_session_.find(map_id);
    if (it != map_to_session_.end()) {
        return sessions_.at(it->second);
    }
    
    auto session = std::make_shared<GameSession>(map, ioc_);
    auto session_id = session->GetId();
    
    sessions_.emplace(session_id, session);
    map_to_session_.emplace(map_id, session_id);
    session_players_.emplace(session_id, std::vector<std::weak_ptr<Player>>());
    
    return session;
}

void GameSessionManager::AddPlayerToSession(std::shared_ptr<Player> player,
                                           std::shared_ptr<GameSession> session,
                                           bool randomize_spawn_points) {
    auto session_id = session->GetId();
    
    auto& players = session_players_[session_id];
    players.push_back(player);
    
    player->SetGameSession(session);
    player->CreateDog(player->GetName(), *session->GetMap(), randomize_spawn_points);
}

const std::vector<std::weak_ptr<Player>>& GameSessionManager::GetPlayersInSession(const GameSession::Id& session_id) const {
    static const std::vector<std::weak_ptr<Player>> empty_players;
    
    auto it = session_players_.find(session_id);
    if (it == session_players_.end()) {
        return empty_players;
    }
    
    return it->second;
}

void GameSessionManager::UpdateAllSessions(const std::chrono::milliseconds& delta_time) {
    for (auto& [session_id, players] : session_players_) {
        for (auto& weak_player : players) {
            if (auto player = weak_player.lock()) {
                player->MoveDog(delta_time);
            }
        }
    }
}

}