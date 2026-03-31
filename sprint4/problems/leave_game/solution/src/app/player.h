#pragma once
#include "tagged.h"

#include <string>
#include <chrono>
#include <memory>

namespace model {
class Dog;
}

namespace app {

class GameSession;

class Player {
    inline static size_t max_id_cont_ = 0;
public:
    using Id = util::Tagged<size_t, Player>;
    
    Player(std::string name) : 
        id_(Id{Player::max_id_cont_++}),
        name_(name),
        join_time_(std::chrono::steady_clock::now()) {};
        
    Player(Id id, std::string name) :
        id_(id),
        name_(name),
        join_time_(std::chrono::steady_clock::now()) {
        if(*id_ >= Player::max_id_cont_){
            Player::max_id_cont_ = *id_ + 1;
        }
    };
    
    Player(const Player& other) = default;
    Player(Player&& other) = default;
    Player& operator = (const Player& other) = default;
    Player& operator = (Player&& other) = default;
    virtual ~Player() = default;

    const Id& GetId() const;
    const std::string& GetName() const;
    std::string GetGameSessionId() const;
    std::shared_ptr<GameSession> GetGameSession();
    void SetGameSession(std::shared_ptr<GameSession> session);
    std::weak_ptr<model::Dog> GetDog() const;  // <-- ДОБАВЛЕН const
    void SetDog(std::weak_ptr<model::Dog> dog);
    
    std::chrono::steady_clock::time_point GetJoinTime() const { return join_time_; }
    void SetJoinTime(std::chrono::steady_clock::time_point time) { join_time_ = time; }
    
    static size_t GetMaxId() { return max_id_cont_; }
    static void ResetMaxId(size_t new_max) { max_id_cont_ = new_max; }
    
    void UpdatePlayerCounter() {
        if (*id_ >= max_id_cont_) {
            max_id_cont_ = *id_ + 1;
        }
    }
    
private:
    Id id_;
    std::string name_;
    std::shared_ptr<GameSession> session_;
    std::weak_ptr<model::Dog> dog_;
    std::chrono::steady_clock::time_point join_time_;
};

}