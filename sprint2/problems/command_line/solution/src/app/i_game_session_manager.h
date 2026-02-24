#pragma once
#include "player.h"
#include "game_session.h"
#include "map.h"
#include <vector>
#include <memory>
#include <chrono>

namespace app {

class IGameSessionManager {
public:
    virtual ~IGameSessionManager() = default;
    
    virtual std::shared_ptr<GameSession> FindOrCreateSession(std::shared_ptr<model::Map> map) = 0;
    virtual void AddPlayerToSession(std::shared_ptr<Player> player, 
                                   std::shared_ptr<GameSession> session,
                                   bool randomize_spawn_points) = 0;
    virtual const std::vector<std::weak_ptr<Player>>& GetPlayersInSession(const GameSession::Id& session_id) const = 0;
    virtual void UpdateAllSessions(const std::chrono::milliseconds& delta_time) = 0;
};

}