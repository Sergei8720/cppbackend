#pragma once
#include "i_game_session_manager.h"
#include "game_session.h"
#include "player.h"
#include <unordered_map>
#include <memory>

namespace app {

class GameSessionManager : public IGameSessionManager {
public:
    explicit GameSessionManager(net::io_context& ioc);
    
    std::shared_ptr<GameSession> FindOrCreateSession(std::shared_ptr<model::Map> map) override;
    void AddPlayerToSession(std::shared_ptr<Player> player, 
                           std::shared_ptr<GameSession> session,
                           bool randomize_spawn_points) override;
    const std::vector<std::weak_ptr<Player>>& GetPlayersInSession(const GameSession::Id& session_id) const override;
    void UpdateAllSessions(const std::chrono::milliseconds& delta_time) override;
    
private:
    using SessionMap = std::unordered_map<GameSession::Id, 
                                          std::shared_ptr<GameSession>, 
                                          util::TaggedHasher<GameSession::Id>>;
    using SessionPlayersMap = std::unordered_map<GameSession::Id,
                                                 std::vector<std::weak_ptr<Player>>,
                                                 util::TaggedHasher<GameSession::Id>>;
    using MapToSessionMap = std::unordered_map<model::Map::Id, 
                                               GameSession::Id, 
                                               util::TaggedHasher<model::Map::Id>>;
    
    net::io_context& ioc_;
    SessionMap sessions_;
    SessionPlayersMap session_players_;
    MapToSessionMap map_to_session_;
};

}