#include "game_session.h"
#include "player.h"
#include "random_generators.h"
#include "support_types.h"
#include "logger.h"

namespace app {

GameSession::GameSession(std::shared_ptr<model::Map> map,
                         const TimeInterval& period_of_update_game_state,
                         const model::LootGeneratorConfig& loot_gen_cfg,
                         net::io_context& ioc,
                         double dog_retirement_time_seconds)
    : map_(map)
    , ioc_(ioc)
    , strand_(std::make_shared<SessionStrand>(net::make_strand(ioc_)))
    , id_(*(map->GetId()))
    , loot_generator_(
        TimeInterval(static_cast<uint64_t>(
            loot_gen_cfg.period * model::MILLISECONDS_IN_SECOND)
        ),
        loot_gen_cfg.probability)
    , period_of_update_game_state_(period_of_update_game_state)
    , dog_retirement_timeout_(std::chrono::milliseconds(static_cast<int64_t>(dog_retirement_time_seconds * 1000))) {
    BOOST_LOG_TRIVIAL(info) << "GameSession created for map " << *map->GetId() 
                            << " with retirement_timeout=" << dog_retirement_timeout_.count() << "ms";
}

void GameSession::Run() {
    if(period_of_update_game_state_.count() != 0){
        update_game_state_ticker_ = std::make_shared<time_m::Ticker>(
            strand_,
            period_of_update_game_state_,
            [self = shared_from_this()](const TimeInterval& delta_time) {
                self->UpdateGameState(delta_time);
            }
        );
        update_game_state_ticker_->Start();
    }
    generate_loot_ticker_ = std::make_shared<time_m::Ticker>(
        strand_,
        loot_generator_.GetPeriod(),
        [self = shared_from_this()](const TimeInterval& delta_time) {
                self->GenerateLoot(delta_time);
        }
    );
    generate_loot_ticker_->Start();
};

const GameSession::Id& GameSession::GetId() const noexcept {
    return id_;
}

const std::shared_ptr<model::Map> GameSession::GetMap() {
    return map_;
};

std::shared_ptr<GameSession::SessionStrand> GameSession::GetStrand() {
    return strand_;
};

std::weak_ptr<model::Dog> GameSession::CreateDog(const std::string& dog_name, const model::Map& map, bool randomize_spawn_points){
    auto dog = std::make_shared<model::Dog>(dog_name, map_->GetBagCapacity());
    if(randomize_spawn_points) {
        LocateDogInRandomPositionOnMap(dog);
    } else {
        LocateDogInStartPointOnMap(dog);
    }
    dogs_[dog->GetId()] = dog;
    
    auto now = std::chrono::steady_clock::now();
    retirement_tracker_.UpdateActivity(*dog->GetId(), now);
    
    dog_previous_positions_[*dog->GetId()] = dog->GetPosition();
    
    dog_idle_accumulated_time_[*dog->GetId()] = TimeInterval{0};
    
    BOOST_LOG_TRIVIAL(info) << "Dog created: id=" << *dog->GetId() 
                            << " name=" << dog_name 
                            << " position=(" << dog->GetPosition().x << "," << dog->GetPosition().y << ")";
    
    net::dispatch(*strand_, [self = shared_from_this()]{
        self->GenerateLoot(self->loot_generator_.GetPeriod());
    });

    return dog;
};

const GameSession::LostObjects& GameSession::GetLostObjects() {
    return lost_objects_;
};

void GameSession::AddLostObject(std::shared_ptr<model::LostObject> lost_object) {
    lost_objects_[lost_object->GetId()] = lost_object;
};

void GameSession::AddDog(std::shared_ptr<model::Dog> dog) {
    dogs_[dog->GetId()] = dog;
    auto now = std::chrono::steady_clock::now();
    retirement_tracker_.UpdateActivity(*dog->GetId(), now);
    dog_previous_positions_[*dog->GetId()] = dog->GetPosition();
    
    dog_idle_accumulated_time_[*dog->GetId()] = TimeInterval{0};
    
    BOOST_LOG_TRIVIAL(info) << "Dog added: id=" << *dog->GetId() << " name=" << dog->GetName();
};

void GameSession::UpdateGameState(const TimeInterval& delta_time) {
    static int call_count = 0;
    call_count++;
    
    if (call_count % 10 == 0) {
        BOOST_LOG_TRIVIAL(debug) << "UpdateGameState #" << call_count << " delta=" << delta_time.count() << "ms";
    }
    
    CheckAndRetireDogs(delta_time);
    
    std::unordered_map<uint64_t, geom::Point2D> old_positions;
    for (const auto& [dog_id, dog] : dogs_) {
        old_positions[*dog_id] = dog->GetPosition();
    }
    
    for (auto& [dog_id, dog] : dogs_) {
        geom::Point2D old_position = dog->GetPosition();
        auto old_velocity = dog->GetVelocity();
        
        auto [new_position, new_velocity] = map_->GetValidMove(
            dog->GetPosition(),
            dog->CalculateNewPosition(delta_time),
            dog->GetVelocity()
        );
        dog->SetPosition(new_position);
        dog->SetVelocity(new_velocity);
        
        bool did_move = (old_position.x != new_position.x || old_position.y != new_position.y);
        
        if (did_move) {
            dog_idle_accumulated_time_[*dog_id] = TimeInterval{0};
            
            auto now = std::chrono::steady_clock::now();
            retirement_tracker_.UpdateActivity(*dog_id, now);
            
            if (call_count % 10 == 0) {
                BOOST_LOG_TRIVIAL(debug) << "Dog " << *dog_id << " moved to (" 
                                         << new_position.x << "," << new_position.y << ")";
            }
        } else if (old_velocity.vx != 0 || old_velocity.vy != 0) {
            auto now = std::chrono::steady_clock::now();
            retirement_tracker_.UpdateActivity(*dog_id, now);
            
            BOOST_LOG_TRIVIAL(debug) << "Dog " << *dog_id << " STOPPED at position (" 
                                    << new_position.x << "," << new_position.y << ")";
        }
        
        dog_previous_positions_[*dog_id] = new_position;
    }
    
    for (const auto& [dog_id, dog] : dogs_) {
        auto old_it = old_positions.find(*dog_id);
        if (old_it != old_positions.end()) {
            bool did_move = (old_it->second.x != dog->GetPosition().x ||
                            old_it->second.y != dog->GetPosition().y);
            
            if (!did_move) {
                dog_idle_accumulated_time_[*dog_id] += delta_time;
                
                if (call_count % 10 == 0) {
                    BOOST_LOG_TRIVIAL(debug) << "Dog " << *dog_id << " idle accumulated: " 
                                             << dog_idle_accumulated_time_[*dog_id].count() << "ms";
                }
            }
        }
    }
    
    HandleLoot();
};

void GameSession::GenerateLoot(const GameSession::TimeInterval& delta_time) {
    auto num_new_lost_obj = loot_generator_.Generate(delta_time,
                                                    lost_objects_.size(),
                                                    dogs_.size());
    for(size_t i = 0; i < num_new_lost_obj; ++i) {
        CreateLostObject();
    }
};

void GameSession::CreateLostObject() {
    auto lost_obj = std::make_shared<model::LostObject>();
    SetRandomLootType(lost_obj);
    LocateLootInRandomPositionOnMap(lost_obj);
    size_t value = map_->GetLootTypeBy(lost_obj->GetType()).value;
    lost_obj->SetValue(value);
    lost_objects_[lost_obj->GetId()] = lost_obj;
};

void GameSession::SetRandomLootType(std::shared_ptr<model::LostObject> loot) {
    auto type = utils::GenerateSizeTFromInterval(0, map_->GetNumberOfLootTypes() - 1);
    loot->SetType(type);
};

void GameSession::LocateLootInRandomPositionOnMap(std::shared_ptr<model::LostObject> loot) {
    loot->SetPosition(map_->GenerateRandomPosition());
};

void GameSession::LocateDogInRandomPositionOnMap(std::shared_ptr<model::Dog> dog) {
    dog->SetPosition(map_->GenerateRandomPosition());
};

void GameSession::LocateDogInStartPointOnMap(std::shared_ptr<model::Dog> dog) {
    auto roads = map_->GetRoads();
    auto road = roads[0];
    dog->SetPosition({static_cast<double>(road.GetStart().x),
                        static_cast<double>(road.GetStart().y)});
};

void GameSession::HandleLoot() {
    std::vector< std::shared_ptr<collision_detector::Item> > items;
    std::vector< std::shared_ptr<model::Dog> > dogs;
    for(auto [id, lost_obj] : lost_objects_) {
        items.push_back(lost_obj);
    }
    for(auto office : map_->GetOffices()) {
        items.push_back(std::make_shared<model::Office>(office));
    }
    for(auto [id, dog] : dogs_) {
        dogs.push_back(dog);
    }
    model::ItemDogProvider provider(std::move(items), std::move(dogs));
    auto collected_loot = collision_detector::FindGatherEvents(provider);
    if(collected_loot.empty()) {
        return;
    }
    for(auto clltd_loot : collected_loot){
        CollectLoot(provider, clltd_loot.item_id, clltd_loot.gatherer_id);
        DropLoot(provider, clltd_loot.item_id, clltd_loot.gatherer_id);
    }
};

void GameSession::CollectLoot(const model::ItemDogProvider& provider,
                    size_t item_id,
                    size_t gatherer_id) {
    auto dog = dogs_[provider.GetDogId(gatherer_id)];
    const auto* const casted_lost_obj = provider.TryCastItemTo<model::LostObject>(item_id);
    if(casted_lost_obj) {
        auto lost_object_id = casted_lost_obj->GetId();
        if(dog->IsFullBag() || !lost_objects_.contains(lost_object_id)) {
            return;
        }
        auto loot = lost_objects_.at(lost_object_id);
        dog->CollectLostObject(loot);
        std::erase_if(lost_objects_, [id = lost_object_id](const auto& item) {
            auto const& [key, value] = item;
            return key == id;
        });
    }
};

void GameSession::DropLoot(const model::ItemDogProvider& provider, size_t item_id, size_t gatherer_id) {
    auto dog = dogs_[provider.GetDogId(gatherer_id)];
    const auto* const casted_office = provider.TryCastItemTo<model::Office>(item_id);
    if(casted_office) {
        if(dog->IsEmptyBag()) {
            return;
        }
        dog->DropLostObjectsFromBag();
    }
};

void GameSession::AddPlayer(std::shared_ptr<Player> player) {
    players_.push_back(player);
    BOOST_LOG_TRIVIAL(info) << "Player added to session: " << player->GetName() << " id=" << *player->GetId();
};

void GameSession::SetRetirementCallback(RetirementCallback callback) { 
    retirement_callback_ = std::move(callback); 
    BOOST_LOG_TRIVIAL(info) << "Retirement callback set";
}

void GameSession::SetTokenFinder(std::function<std::optional<authentication::Token>(size_t)> finder) {
    token_finder_ = std::move(finder);
    BOOST_LOG_TRIVIAL(info) << "Token finder set";
}

GameSession::TimePoint GameSession::GetInactivityStartTime(uint64_t dog_id) const {
    return retirement_tracker_.GetInactivityStartTime(dog_id);
}

std::shared_ptr<Player> GameSession::FindOwnerByDogId(uint64_t dog_id) const {
    for (const auto& player : players_) {
        auto player_dog = player->GetDog().lock();
        if (player_dog && *player_dog->GetId() == dog_id) {
            return player;
        }
    }
    return nullptr;
}

void GameSession::CheckAndRetireDogs(const TimeInterval& delta_time) {
    static int check_count = 0;
    check_count++;
    
    std::vector<model::Dog::Id> dogs_to_remove;
    std::vector<std::shared_ptr<Player>> players_to_remove;
    
    if (check_count % 10 == 0) {
        BOOST_LOG_TRIVIAL(debug) << "CheckAndRetireDogs #" << check_count 
                                 << " dogs=" << dogs_.size()
                                 << " delta=" << delta_time.count() << "ms";
    }
    
    for (const auto& [dog_id, dog] : dogs_) {
        auto owner = FindOwnerByDogId(*dog_id);
        if (!owner) {
            BOOST_LOG_TRIVIAL(warning) << "No owner found for dog " << *dog_id;
            continue;
        }
        
        auto idle_time = dog_idle_accumulated_time_[*dog_id];
        
        bool should_retire = idle_time >= dog_retirement_timeout_;
        
        if (check_count % 10 == 0 || should_retire) {
            BOOST_LOG_TRIVIAL(info) << "DOG CHECK: id=" << *dog_id 
                                    << " name=" << dog->GetName()
                                    << " idle_time=" << idle_time.count() << "ms"
                                    << " timeout=" << dog_retirement_timeout_.count() << "ms"
                                    << " should_retire=" << (should_retire ? "YES" : "no");
        }
        
        if (should_retire) {
            BOOST_LOG_TRIVIAL(info) << ">>> DOG " << *dog_id << " WILL BE RETIRED NOW";
            dogs_to_remove.push_back(dog_id);
            players_to_remove.push_back(owner);
        }
    }
    
    if (dogs_to_remove.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "No dogs to retire in this tick (check_count=" << check_count << ")";
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "Found " << dogs_to_remove.size() << " dogs to retire";
    
    for (size_t i = 0; i < dogs_to_remove.size(); ++i) {
        const auto& dog_id = dogs_to_remove[i];
        const auto& player = players_to_remove[i];
        auto dog = dogs_[dog_id];
        
        auto total_play_time_ms = dog_idle_accumulated_time_[*dog_id].count();
        
        BOOST_LOG_TRIVIAL(info) << "Retiring player: " << player->GetName()
                                << " dog=" << dog->GetName()
                                << " dog_id=" << *dog_id
                                << " score=" << dog->GetScore()
                                << " play_time_ms=" << total_play_time_ms;
        
        std::optional<authentication::Token> token;
        if (token_finder_) {
            token = token_finder_(*player->GetId());
            BOOST_LOG_TRIVIAL(debug) << "Token found: " << (token.has_value() ? "YES" : "NO");
        }
        
        if (retirement_callback_ && token.has_value()) {
            BOOST_LOG_TRIVIAL(info) << "Calling retirement callback for " << player->GetName();
            retirement_callback_(token.value(), *player->GetId(), total_play_time_ms);
            BOOST_LOG_TRIVIAL(info) << "Retirement callback completed";
        } else {
            BOOST_LOG_TRIVIAL(warning) << "Cannot retire: callback=" << (retirement_callback_ ? "yes" : "no")
                                       << " token=" << (token.has_value() ? "yes" : "no");
        }
        
        dog_idle_accumulated_time_.erase(*dog_id);
        retirement_tracker_.RemoveDog(*dog_id);
        dogs_.erase(dog_id);
        dog_previous_positions_.erase(*dog_id);
        
        auto it = std::find(players_.begin(), players_.end(), player);
        if (it != players_.end()) {
            players_.erase(it);
            BOOST_LOG_TRIVIAL(info) << "Player removed, remaining players: " << players_.size();
        }
    }
}

void GameSession::UpdateDogActivity(uint64_t dog_id, const TimePoint& now) {
    retirement_tracker_.UpdateActivity(dog_id, now);
    dog_idle_accumulated_time_[dog_id] = TimeInterval{0};
    BOOST_LOG_TRIVIAL(debug) << "Dog " << dog_id << " activity updated, idle time reset";
}

}