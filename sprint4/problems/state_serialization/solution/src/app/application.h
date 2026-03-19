#pragma once
#include "game.h"
#include "player.h"
#include "player_tokens.h"
#include "tagged.h"
#include "saving_settings.h"
#include "game_session_serialization.h"
#include "ticker.h"

#include <vector>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <optional>

namespace app {

namespace net = boost::asio;

class Application : public std::enable_shared_from_this<Application> {
public:
    using AppStrand = net::strand<net::io_context::executor_type>;

    Application(model::Game game, size_t tick_period, bool randomize_spawn_points, net::io_context& ioc) :
            game_{std::move(game)},
            tick_period_{tick_period},
            randomize_spawn_points_{randomize_spawn_points},
            ioc_{ioc} {
    };
    Application(const Application& other) = delete;
    Application(Application&& other) = delete;
    Application& operator = (const Application& other) = delete;
    Application& operator = (Application&& other) = delete;
    virtual ~Application() = default;

    const model::Game::Maps& ListMap() const noexcept;
    const std::shared_ptr<model::Map> FindMap(const model::Map::Id& id) const noexcept;
    std::tuple<authentication::Token, Player::Id> JoinGame(const std::string& player_name, const model::Map::Id& id);
    const std::vector< std::shared_ptr<Player> >& GetPlayersFromGameSession(const authentication::Token& token);
    bool IsExistPlayer(const authentication::Token& token);
    void SetPlayerAction(const authentication::Token& token, model::Direction direction);
    bool IsManualTimeManagement();
    void UpdateGameState(const std::chrono::milliseconds& delta_time);
    void AddGameSession(std::shared_ptr<GameSession> session);
    std::shared_ptr<GameSession> FindGameSessionBy(const model::Map::Id& id) const noexcept;
    std::shared_ptr<GameSession> FindGameSessionBy(const authentication::Token& token) const noexcept;
    const std::vector< std::shared_ptr<app::GameSession> >& GetSessions();
    
    // Методы для сохранения/восстановления состояния
    void SetupStateSaving(const std::string& state_file_path, size_t save_period_ms);
    void SaveGameState();
    bool LoadGameState(const std::string& state_file_path, net::io_context& ioc);
    
    // Вспомогательные методы для сериализации
    std::optional<authentication::Token> FindTokenByPlayer(const Player::Id& player_id) const;
    void RestorePlayer(const authentication::Token& token, std::shared_ptr<Player> player, 
                       std::shared_ptr<GameSession> session);

private:
    using GameSessionIdHasher = util::TaggedHasher<GameSession::Id>;
    using GameSessionIdToIndex = std::unordered_map<GameSession::Id,
                                                    std::vector< std::shared_ptr<Player> >,
                                                    GameSessionIdHasher>;
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;
    using MapIdToSessionIndex = std::unordered_map<model::Map::Id, size_t, MapIdHasher>;
    using AuthTokenToSessionIndex = std::unordered_map<authentication::Token, std::shared_ptr<GameSession>,
                                                        authentication::TokenHasher>;
    using TokenToPlayer = std::unordered_map< authentication::Token, std::shared_ptr<app::Player>,
                                    authentication::TokenHasher >;
    using GameSessionToTokenPlayerPair = std::unordered_map<std::shared_ptr<GameSession>,
                                                            TokenToPlayer>;
    using PlayerIdToToken = std::unordered_map<Player::Id, authentication::Token, util::TaggedHasher<Player::Id>>;

    model::Game game_;
    std::chrono::milliseconds tick_period_;
    bool randomize_spawn_points_;
    std::vector< std::shared_ptr<Player> > players_;
    GameSessionIdToIndex session_id_to_players_;
    authentication::PlayerTokens player_tokens_;
    net::io_context& ioc_;
    std::vector< std::shared_ptr<app::GameSession> > sessions_;
    MapIdToSessionIndex map_id_to_session_index_;
    AuthTokenToSessionIndex auth_token_to_session_index_;
    GameSessionToTokenPlayerPair game_session_to_token_player_pair_;
    PlayerIdToToken player_id_to_token_;  // Для обратного поиска токена по ID игрока
    
    // Параметры сохранения состояния
    std::optional<std::string> state_file_path_;
    std::chrono::milliseconds save_state_period_{0};
    std::shared_ptr<time_m::Ticker> save_state_ticker_;
    int64_t time_since_last_save_{0};

    std::shared_ptr<Player> CreatePlayer(const std::string& player_name);
    void BoundPlayerAndGameSession(std::shared_ptr<Player> player,
                                    std::shared_ptr<GameSession> session);
    void OnSaveStateTimer(const std::chrono::milliseconds& delta_time);
    
    // Структура для полной сериализации состояния игры
    struct GameStateSerialization {
        std::vector<game_data_ser::GameSessionSerialization> sessions;
        std::vector<game_data_ser::PlayerSerialization> players;
        
        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
            ar& sessions;
            ar& players;
        }
    };
    
    GameStateSerialization GetSerializedState();
    bool RestoreFromSerializedState(const GameStateSerialization& state, net::io_context& ioc);
};

}