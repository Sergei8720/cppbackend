#include "application.h"
#include "game_session_manager.h"

namespace app {

using namespace std::literals;

Application::Application(model::Game game, size_t tick_period, bool randomize_spawn_points, net::io_context& ioc)
    : game_{std::move(game)}
    , tick_period_{tick_period}
    , randomize_spawn_points_{randomize_spawn_points}
    , ioc_{ioc}
    , strand_{std::make_shared<AppStrand>(net::make_strand(ioc))}
    , session_manager_{std::make_unique<GameSessionManager>(ioc)} {
    
    if(tick_period_.count() != 0){
        ticker_ = std::make_shared<time_m::Ticker>(
            strand_,
            tick_period_,
            [this](const std::chrono::milliseconds& delta) {
                UpdateGameState(delta);
            }
        );
        ticker_->Start();
    }
}

const model::Game::Maps& Application::ListMap() const noexcept {
    return game_.GetMaps();
}

const std::shared_ptr<model::Map> Application::FindMap(const model::Map::Id& id) const noexcept {
    return game_.FindMap(id);
}

std::tuple<authentication::Token, Player::Id> Application::JoinGame(
        const std::string& player_name,
        const model::Map::Id& id) {
    
    auto map = game_.FindMap(id);
    if (!map) {
        throw std::invalid_argument("Map not found");
    }
    
    auto player = CreatePlayer(player_name);
    auto token = player_tokens_.AddPlayer(player);
    
    auto game_session = session_manager_->FindOrCreateSession(map);
    session_manager_->AddPlayerToSession(player, game_session, randomize_spawn_points_);
    
    return std::make_tuple(token, player->GetId());
}

std::shared_ptr<Player> Application::CreatePlayer(const std::string& player_name) {
    auto player = std::make_shared<Player>(player_name);
    players_.push_back(player);
    return player;
}

const std::vector<std::weak_ptr<Player>>& Application::GetPlayersFromGameSession(const authentication::Token& token) {
    static const std::vector<std::weak_ptr<Player>> emptyPlayerList;
    
    auto player = player_tokens_.FindPlayerBy(token).lock();
    if (!player) {
        return emptyPlayerList;
    }
    
    return session_manager_->GetPlayersInSession(player->GetGameSessionId());
}

bool Application::IsExistPlayer(const authentication::Token& token) const noexcept {
    return !player_tokens_.FindPlayerBy(token).expired();
}

void Application::SetPlayerAction(const authentication::Token& token, model::Direction direction) {
    auto player = player_tokens_.FindPlayerBy(token).lock();
    if (!player) {
        return;
    }
    
    auto session = player->GetGameSession();
    double velocity = session->GetMap()->GetDogVelocity();
    player->GetDog()->SetAction(direction, velocity);
}

bool Application::IsManualTimeManagement() const noexcept {
    return tick_period_.count() == 0;
}

void Application::UpdateGameState(const std::chrono::milliseconds& delta_time) {
    session_manager_->UpdateAllSessions(delta_time);
}

std::shared_ptr<Application::AppStrand> Application::GetStrand() const noexcept {
    return strand_;
}

}