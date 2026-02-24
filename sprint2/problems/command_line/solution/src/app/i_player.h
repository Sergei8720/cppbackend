#pragma once
#include "game_session.h"
#include "dog.h"

namespace app {

class IPlayer {
public:
    virtual ~IPlayer() = default;
    
    virtual const Player::Id& GetId() const = 0;
    virtual const std::string& GetName() const = 0;
    virtual const GameSession::Id& GetGameSessionId() const = 0;
    virtual std::shared_ptr<GameSession> GetGameSession() = 0;
    virtual void SetGameSession(std::shared_ptr<GameSession> session) = 0;
    virtual std::shared_ptr<model::Dog> GetDog() = 0;
    virtual void CreateDog(const std::string& dog_name, const model::Map& map, bool randomize_spawn_points) = 0;
    virtual void MoveDog(const std::chrono::milliseconds& delta_time) = 0;
};

}