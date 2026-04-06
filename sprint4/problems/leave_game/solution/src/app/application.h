#pragma once
#include "game.h"
#include "player.h"
#include "player_tokens.h"
#include "tagged.h"
#include "saving_settings.h"
#include "game_session_serialization.h"
#include "ticker.h"
#include "database/player_record.h"
#include "database/connection_pool.h"
#include <vector>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <optional>
#include <atomic>

namespace app {

namespace net = boost::asio;

class StateSerializer;

class Application  : public std::enable_shared_from_this<Application>{
public:
    using AppStrand = net::strand<net::io_context::executor_type>;

    Application(model::Game game, size_t tick_period, bool randomize_spawn_points, net::io_context& ioc) :
            game_{std::move(game)},
            tick_period_{tick_period},
            randomize_spawn_points_{randomize_spawn_points},
            ioc_{ioc},
            save_period_counter_{0} {
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
    const std::vector< std::shared_ptr<app::GameSession> >& GetSessions() const { return sessions_; }
    const std::vector< std::shared_ptr<Player> >& GetAllPlayers() const { return players_; }
    void SaveGame();
    std::optional<authentication::Token> FindTokenByPlayer(size_t player_id) const;
    std::shared_ptr<Player> FindPlayerById(size_t player_id) const;
    void RestorePlayer(const authentication::Token& token, 
                       std::shared_ptr<Player> player,
                       std::shared_ptr<GameSession> session);
    std::chrono::milliseconds GetTickPeriod() const { return tick_period_; }
    const model::LootGeneratorConfig& GetLootGeneratorConfig() const { return game_.GetLootGeneratorConfig(); }
    void SetSavingSettings(const saving::SavingSettings& settings) { saving_settings_ = settings; }
    void SetStateSerializer(std::shared_ptr<StateSerializer> serializer) { state_serializer_ = serializer; }
    bool ShouldSaveState() const { return saving_settings_.period.has_value() && saving_settings_.period.value().count() > 0; }
    
    void DumpPlayerTokens() const {
        BOOST_LOG_TRIVIAL(info) << "=== PlayerTokens dump ===";
        BOOST_LOG_TRIVIAL(info) << "Total tokens in player_tokens_: " << player_tokens_.Size();
        BOOST_LOG_TRIVIAL(info) << "Total players: " << players_.size();
        for (const auto& player : players_) {
            auto token_opt = FindTokenByPlayer(*player->GetId());
            if (token_opt.has_value()) {
                BOOST_LOG_TRIVIAL(info) << "  Player id=" << *player->GetId() 
                                       << " name=" << player->GetName()
                                       << " token=" << *token_opt.value();
            } else {
                BOOST_LOG_TRIVIAL(warning) << "  Player id=" << *player->GetId() 
                                           << " name=" << player->GetName()
                                           << " has NO token!";
            }
        }
    }
    
    double GetDogRetirementTime() const { return game_.GetDogRetirementTime(); }
    void RemovePlayerAndSaveRecord(const authentication::Token& token, 
                                   std::shared_ptr<Player> player,
                                   int64_t play_time_ms);
    
    void SetConnectionPool(std::shared_ptr<database::ConnectionPool> pool) { db_pool_ = pool; }
    std::shared_ptr<database::ConnectionPool> GetConnectionPool() const { return db_pool_; }
    
    void UpdateDogGameTime(uint64_t dog_id, std::chrono::milliseconds delta);
    void UpdateDogInactiveTime(uint64_t dog_id, std::chrono::milliseconds delta, bool is_active);
    std::chrono::milliseconds GetDogGameTime(uint64_t dog_id) const;
    void RemoveDogTimeTracking(uint64_t dog_id);
	
	void CommitGameRecords(const std::vector<database::PlayerRecord>& player_records);
	void RemoveInactivePlayers(const GameSession::Id& session_id);
    
private:
    using GameSessionIdToPlayers = std::unordered_map<std::string,
                                                    std::vector< std::shared_ptr<Player> >>;
    using MapIdHasher = util::TaggedHasher<model::Map::Id>;
    using MapIdToSessionIndex = std::unordered_map<model::Map::Id, size_t, MapIdHasher>;
    using AuthTokenToSessionIndex = std::unordered_map<authentication::Token, std::shared_ptr<GameSession>,
                                                        authentication::TokenHasher>;
    using TokenToPlayer = std::unordered_map< authentication::Token, std::shared_ptr<app::Player>,
                                    authentication::TokenHasher >;
    using GameSessionToTokenPlayerPair = std::unordered_map<std::shared_ptr<GameSession>,
                                                            TokenToPlayer>;
    using PlayerIdToToken = std::unordered_map<size_t, authentication::Token>;

    model::Game game_;
    std::chrono::milliseconds tick_period_;
    bool randomize_spawn_points_;
    std::vector< std::shared_ptr<Player> > players_;
    GameSessionIdToPlayers session_id_to_players_;
    authentication::PlayerTokens player_tokens_;
    net::io_context& ioc_;
    std::vector< std::shared_ptr<app::GameSession> > sessions_;
    MapIdToSessionIndex map_id_to_session_index_;
    AuthTokenToSessionIndex auth_token_to_session_index_;
    saving::SavingSettings saving_settings_;
    GameSessionToTokenPlayerPair game_session_to_token_player_pair_;
    std::shared_ptr<time_m::Ticker> save_game_ticker_;
    PlayerIdToToken player_id_to_token_;
    int save_period_counter_{0};
    
    std::shared_ptr<StateSerializer> state_serializer_;
    std::shared_ptr<database::ConnectionPool> db_pool_;
    
    std::unordered_map<uint64_t, std::chrono::milliseconds> dog_game_time_;
    std::unordered_map<uint64_t, std::chrono::milliseconds> dog_inactive_time_;

    std::shared_ptr<Player> CreatePlayer(const std::string& player_name);
    void BoundPlayerAndGameSession(std::shared_ptr<Player> player,
                                    std::shared_ptr<GameSession> session);
    void SaveGameState(const std::chrono::milliseconds& delta_time);
    std::vector<game_data_ser::GameSessionSerialization> GetSerializedData();
};

}