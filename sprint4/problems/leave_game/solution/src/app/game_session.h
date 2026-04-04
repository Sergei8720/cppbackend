#pragma once
#include "map.h"
#include "dog.h"
#include "tagged.h"
#include "loot_generator_config.h"
#include "loot_generator.h"
#include "lost_object.h"
#include "ticker.h"
#include "model_invariants.h"
#include "item_dog_provider.h"
#include "retirement/retirement_tracker.h"
#include "player_tokens.h"

#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

namespace app {

namespace net = boost::asio;

class Player;

class GameSession : public std::enable_shared_from_this<GameSession>  {
public:
    using SessionStrand = net::strand<net::io_context::executor_type>;
    using Id = util::Tagged<std::string, GameSession>;
    using TimeInterval = std::chrono::milliseconds;
    using TimePoint = std::chrono::steady_clock::time_point;
    using LostObjectIdHasher = util::TaggedHasher<model::LostObject::Id>;
    using LostObjects = std::unordered_map<model::LostObject::Id,
                                            std::shared_ptr<model::LostObject>,
                                            LostObjectIdHasher>;
    using DogIdHasher = util::TaggedHasher<model::Dog::Id>;
    using Dogs = std::unordered_map<model::Dog::Id,
                                    std::shared_ptr<model::Dog>,
                                    DogIdHasher>;
    
    using RetirementCallback = std::function<void(const authentication::Token&, 
                                                   size_t player_id, 
                                                   int64_t play_time_ms)>;

    GameSession(std::shared_ptr<model::Map> map,
                    const TimeInterval& period_of_update_game_state,
                    const model::LootGeneratorConfig& loot_gen_cfg,
                    net::io_context& ioc,
                    double dog_retirement_time_seconds = 60.0);
    
    void Run();
    
    const Id& GetId() const noexcept;
    const std::shared_ptr<model::Map> GetMap();
    std::shared_ptr<SessionStrand> GetStrand();
    std::weak_ptr<model::Dog> CreateDog(const std::string& dog_name,
                                        const model::Map& map,
                                        bool randomize_spawn_points);
    void UpdateGameState(const TimeInterval& delta_time);
    const LostObjects& GetLostObjects();
    void AddLostObject(std::shared_ptr<model::LostObject> lost_object);
    void AddDog(std::shared_ptr<model::Dog> dog);
    void AddPlayer(std::shared_ptr<Player> player);
    const std::vector<std::shared_ptr<Player>>& GetPlayers() const { return players_; }
    const Dogs& GetDogs() const { return dogs_; }
    
    void RestoreDog(std::shared_ptr<model::Dog> dog) {
        dogs_[dog->GetId()] = dog;
    }
    
    std::chrono::milliseconds GetDogRetirementTimeout() const { return dog_retirement_timeout_; }
    
    void SetRetirementCallback(RetirementCallback callback);
    void SetTokenFinder(std::function<std::optional<authentication::Token>(size_t)> finder);
    
    TimePoint GetInactivityStartTime(uint64_t dog_id) const;
    
    // НОВЫЙ МЕТОД: обновление активности собаки по действию игрока
    void UpdateDogActivity(uint64_t dog_id, const TimePoint& now);
    
private:
    std::shared_ptr<model::Map> map_;
    net::io_context& ioc_;
    std::shared_ptr<SessionStrand> strand_;
    Id id_;
    loot_gen::LootGenerator loot_generator_;
    Dogs dogs_;
    LostObjects lost_objects_;
    TimeInterval period_of_update_game_state_;
    std::shared_ptr<time_m::Ticker> update_game_state_ticker_;
    std::shared_ptr<time_m::Ticker> generate_loot_ticker_;
    std::vector<std::shared_ptr<Player>> players_;
    
    retirement::RetirementTracker retirement_tracker_;
    std::chrono::milliseconds dog_retirement_timeout_;
    
    RetirementCallback retirement_callback_;
    std::function<std::optional<authentication::Token>(size_t)> token_finder_;
    
    // Для отслеживания позиции собак
    std::unordered_map<uint64_t, geom::Point2D> dog_previous_positions_;
    
    void GenerateLoot(const TimeInterval& delta_time);
    void CreateLostObject();
    void SetRandomLootType(std::shared_ptr<model::LostObject> loot);
    void LocateLootInRandomPositionOnMap(std::shared_ptr<model::LostObject> loot);
    void LocateDogInRandomPositionOnMap(std::shared_ptr<model::Dog> dog);
    void LocateDogInStartPointOnMap(std::shared_ptr<model::Dog> dog);
    void HandleLoot();
    void CollectLoot(const model::ItemDogProvider& provider, size_t item_id, size_t gatherer_id);
    void DropLoot(const model::ItemDogProvider& provider, size_t item_id, size_t gatherer_id);
    void CheckAndRetireDogs(const TimeInterval& delta_time);
};

}