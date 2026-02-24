#pragma once
#include "game.h"
#include "player_tokens.h"
#include <tuple>
#include <memory>
#include <vector>

namespace app {

class IApplication {
public:
    virtual ~IApplication() = default;
    
    virtual const model::Game::Maps& ListMap() const noexcept = 0;
    virtual const std::shared_ptr<model::Map> FindMap(const model::Map::Id& id) const noexcept = 0;
    virtual std::tuple<authentication::Token, Player::Id> JoinGame(
        const std::string& player_name, 
        const model::Map::Id& id) = 0;
    virtual const std::vector<std::weak_ptr<Player>>& GetPlayersFromGameSession(
        const authentication::Token& token) = 0;
    virtual bool IsExistPlayer(const authentication::Token& token) const noexcept = 0;
    virtual void SetPlayerAction(const authentication::Token& token, model::Direction direction) = 0;
    virtual std::shared_ptr<Application::AppStrand> GetStrand() const noexcept = 0;
    virtual bool IsManualTimeManagement() const noexcept = 0;
    virtual void UpdateGameState(const std::chrono::milliseconds& delta_time) = 0;
};

}