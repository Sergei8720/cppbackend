#pragma once
#include "game.h"
#include "player.h"
#include "player_tokens.h"
#include "tagged.h"
#include "ticker.h"
#include "i_game_session_manager.h"

#include <vector>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <functional>

namespace app {

namespace net = boost::asio;

class Application {
public:
    using AppStrand = net::strand<net::io_context::executor_type>;

    Application(model::Game game, size_t tick_period, bool randomize_spawn_points, net::io_context& ioc);
    ~Application() = default;
    
    Application(const Application& other) = delete;
    Application(Application&& other) = delete;
    Application& operator = (const Application& other) = delete;
    Application& operator = (Application&& other) = delete;

    const model::Game::Maps& ListMap() const noexcept;
    const std::shared_ptr<model::Map> FindMap(const model::Map::Id& id) const noexcept;
    std::tuple<authentication::Token, Player::Id> JoinGame(
        const std::string& player_name, 
        const model::Map::Id& id);
    const std::vector<std::weak_ptr<Player>>& GetPlayersFromGameSession(
        const authentication::Token& token);
    bool IsExistPlayer(const authentication::Token& token) const noexcept;
    void SetPlayerAction(const authentication::Token& token, model::Direction direction);
    std::shared_ptr<AppStrand> GetStrand() const noexcept;
    bool IsManualTimeManagement() const noexcept;
    void UpdateGameState(const std::chrono::milliseconds& delta_time);

private:
    std::shared_ptr<Player> CreatePlayer(const std::string& player_name);

    model::Game game_;
    std::chrono::milliseconds tick_period_;
    bool randomize_spawn_points_;
    std::vector<std::shared_ptr<Player>> players_;
    authentication::PlayerTokens player_tokens_;
    net::io_context& ioc_;
    std::shared_ptr<AppStrand> strand_;
    std::shared_ptr<time_m::Ticker> ticker_;
    std::unique_ptr<IGameSessionManager> session_manager_;
};

}