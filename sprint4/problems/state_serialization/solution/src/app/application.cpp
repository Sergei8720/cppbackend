#include "application.h"
#include "player_tokens.h"
#include "random_generators.h"
#include "logger.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/thread/future.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>

namespace app {

using namespace std::literals;

const model::Game::Maps& Application::ListMap() const noexcept {
    return game_.GetMaps();
};

const std::shared_ptr<model::Map> Application::FindMap(const model::Map::Id& id) const noexcept {
    return game_.FindMap(id);
};

std::tuple<authentication::Token, Player::Id> Application::JoinGame(
        const std::string& player_name,
        const model::Map::Id& id) {
    auto player = CreatePlayer(player_name);
    auto token = player_tokens_.AddPlayer(player);
    
    // Сохраняем обратную связь ID игрока -> токен
    player_id_to_token_[player->GetId()] = token;
    
    std::shared_ptr<GameSession> game_session = FindGameSessionBy(id);
    if(!game_session){
        game_session = std::make_shared<GameSession>(game_.FindMap(id), tick_period_, game_.GetLootGeneratorConfig(), ioc_);
        AddGameSession(game_session);
        game_session->Run();
    }
    auth_token_to_session_index_[token] = game_session;
    BoundPlayerAndGameSession(player, game_session);
    game_session_to_token_player_pair_[game_session][token] = player;
    return std::tie(token, player->GetId());
};

std::shared_ptr<Player> Application::CreatePlayer(const std::string& player_name) {
    auto player = std::make_shared<Player>(player_name);
    players_.push_back(player);
    return player;
};

void Application::BoundPlayerAndGameSession(std::shared_ptr<Player> player,
                                    std::shared_ptr<GameSession> session){
    session_id_to_players_[session->GetId()].push_back(player);
    player->SetGameSession(session);
    auto dog = session->CreateDog(player->GetName(), *(session->GetMap()), randomize_spawn_points_);
    player->SetDog(dog);
};

const std::vector< std::shared_ptr<Player> >& Application::GetPlayersFromGameSession(const authentication::Token& token) {
    static const std::vector< std::shared_ptr<Player> > emptyPlayerList;
    auto player = player_tokens_.FindPlayerBy(token);
    if (!player) {
        return emptyPlayerList;
    }
    auto session_id = player->GetGameSessionId();
    if(!session_id_to_players_.contains(session_id)) {
        return emptyPlayerList;
    }
    return session_id_to_players_[session_id];
};

bool Application::IsExistPlayer(const authentication::Token& token) {
    return static_cast<bool>(player_tokens_.FindPlayerBy(token));
};

void Application::SetPlayerAction(const authentication::Token& token, model::Direction direction) {
    auto player = player_tokens_.FindPlayerBy(token);
    if (!player) {
        return;
    }
    auto dog = player->GetDog().lock();
    if (!dog) {
        return;
    }
    double velocity = player->GetGameSession()->GetMap()->GetDogVelocity();
    dog->SetAction(direction, velocity);
};

bool Application::IsManualTimeManagement() {
    return tick_period_.count() == 0;
};

void Application::UpdateGameState(const std::chrono::milliseconds& delta_time) {
    // Обновляем состояние всех сессий
    for(auto session : sessions_) {
        boost::promise<void> res_promise;
        auto res_future = res_promise.get_future();
        net::dispatch(*(session->GetStrand()),
            [session, &delta_time, &res_promise] {
                session->UpdateGameState(delta_time);
                res_promise.set_value();
            }
        );
        res_future.get();
    }
    
    // При ручном управлении временем и включенном сохранении
    if (state_file_path_ && save_state_period_.count() > 0 && IsManualTimeManagement()) {
        time_since_last_save_ += delta_time.count();
        if (time_since_last_save_ >= save_state_period_.count()) {
            SaveGameState();
            time_since_last_save_ = 0;
        }
    }
};

void Application::AddGameSession(std::shared_ptr<GameSession> session) {
    const size_t index = sessions_.size();
    if (auto [it, inserted] = map_id_to_session_index_.emplace(session->GetMap()->GetId(), index); !inserted) {
        throw std::invalid_argument("Game session with map id "s + *(session->GetMap()->GetId()) + " already exists"s);
    } else {
        try {
            sessions_.push_back(session);
        } catch (...) {
            map_id_to_session_index_.erase(it);
            throw;
        }
    }
};

std::shared_ptr<GameSession> Application::FindGameSessionBy(const model::Map::Id& id) const noexcept {
    if (auto it = map_id_to_session_index_.find(id); it != map_id_to_session_index_.end()) {
        return sessions_.at(it->second);
    }
    return nullptr;
};

std::shared_ptr<GameSession> Application::FindGameSessionBy(const authentication::Token& token) const noexcept {
    if (auto it = auth_token_to_session_index_.find(token); it != auth_token_to_session_index_.end()) {
        return it->second;
    }
    return nullptr;
};

const std::vector< std::shared_ptr<GameSession> >& Application::GetSessions() {
    return sessions_;
};

void Application::SetupStateSaving(const std::string& state_file_path, size_t save_period_ms) {
    state_file_path_ = state_file_path;
    save_state_period_ = std::chrono::milliseconds(save_period_ms);
    
    // Если период сохранения > 0 и не ручное управление временем, запускаем таймер
    if (save_state_period_.count() > 0 && !IsManualTimeManagement()) {
        save_state_ticker_ = std::make_shared<time_m::Ticker>(
            ioc_,
            save_state_period_,
            [self = shared_from_this()](const std::chrono::milliseconds& delta_time) {
                self->OnSaveStateTimer(delta_time);
            }
        );
        save_state_ticker_->Start();
        time_since_last_save_ = 0;
    }
};

void Application::OnSaveStateTimer(const std::chrono::milliseconds& delta_time) {
    time_since_last_save_ += delta_time.count();
    if (time_since_last_save_ >= save_state_period_.count()) {
        SaveGameState();
        time_since_last_save_ = 0;
    }
};

void Application::SaveGameState() {
    if (!state_file_path_) {
        return;
    }
    
    try {
        BOOST_LOG_TRIVIAL(info) << "Saving game state to " << *state_file_path_;
        
        // Получаем сериализованное состояние
        auto state = GetSerializedState();
        
        // Сохраняем во временный файл для атомарности
        std::string temp_file = *state_file_path_ + ".tmp";
        {
            std::ofstream ofs(temp_file);
            if (!ofs.is_open()) {
                BOOST_LOG_TRIVIAL(error) << "Failed to open temporary file for writing: " << temp_file;
                return;
            }
            boost::archive::text_oarchive oa(ofs);
            oa << state;
        }
        
        // Атомарно переименовываем временный файл в целевой
        boost::system::error_code ec;
        boost::filesystem::rename(temp_file, *state_file_path_, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to rename temporary file: " << ec.message();
            boost::filesystem::remove(temp_file);
            return;
        }
        
        BOOST_LOG_TRIVIAL(info) << "Game state saved successfully";
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to save game state: " << e.what();
    }
};

Application::GameStateSerialization Application::GetSerializedState() {
    using game_data_ser::GameSessionSerialization;
    using game_data_ser::PlayerSerialization;
    
    GameStateSerialization state;
    
    // Сохраняем все игровые сессии
    for (const auto& session_ptr : sessions_) {
        boost::promise<GameSessionSerialization> promise;
        auto future = promise.get_future();
        
        net::dispatch(*(session_ptr->GetStrand()),
            [this, session_ptr, &promise]() {
                // Собираем токены игроков для этой сессии
                TokenToPlayer token_to_player;
                if (auto it = game_session_to_token_player_pair_.find(session_ptr); 
                    it != game_session_to_token_player_pair_.end()) {
                    token_to_player = it->second;
                }
                promise.set_value(GameSessionSerialization(*session_ptr, token_to_player));
            }
        );
        
        state.sessions.push_back(future.get());
    }
    
    // Сохраняем всех игроков с их токенами (на случай, если есть игроки без сессий)
    for (const auto& player : players_) {
        auto token_opt = FindTokenByPlayer(player->GetId());
        if (token_opt) {
            state.players.emplace_back(*player, *token_opt);
        }
    }
    
    return state;
};

std::optional<authentication::Token> Application::FindTokenByPlayer(const Player::Id& player_id) const {
    if (auto it = player_id_to_token_.find(player_id); it != player_id_to_token_.end()) {
        return it->second;
    }
    return std::nullopt;
};

bool Application::LoadGameState(const std::string& state_file_path, net::io_context& ioc) {
    try {
        if (!boost::filesystem::exists(state_file_path)) {
            BOOST_LOG_TRIVIAL(info) << "State file not found, starting fresh: " << state_file_path;
            return false;
        }
        
        BOOST_LOG_TRIVIAL(info) << "Loading game state from " << state_file_path;
        
        GameStateSerialization state;
        {
            std::ifstream ifs(state_file_path);
            if (!ifs.is_open()) {
                BOOST_LOG_TRIVIAL(error) << "Failed to open state file for reading: " << state_file_path;
                return false;
            }
            boost::archive::text_iarchive ia(ifs);
            ia >> state;
        }
        
        bool restored = RestoreFromSerializedState(state, ioc);
        if (restored) {
            BOOST_LOG_TRIVIAL(info) << "Game state loaded successfully";
        } else {
            BOOST_LOG_TRIVIAL(error) << "Failed to restore game state";
        }
        return restored;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Exception while loading game state: " << e.what();
        return false;
    }
};

bool Application::RestoreFromSerializedState(const GameStateSerialization& state, net::io_context& ioc) {
    try {
        using game_data_ser::GameSessionSerialization;
        using game_data_ser::PlayerSerialization;
        
        // Очищаем текущее состояние
        sessions_.clear();
        players_.clear();
        map_id_to_session_index_.clear();
        auth_token_to_session_index_.clear();
        game_session_to_token_player_pair_.clear();
        player_id_to_token_.clear();
        session_id_to_players_.clear();
        
        // Восстанавливаем сессии
        for (const auto& session_ser : state.sessions) {
            auto map_id = session_ser.RestoreMapId();
            auto map = game_.FindMap(map_id);
            if (!map) {
                BOOST_LOG_TRIVIAL(error) << "Map not found for restored session: " << *map_id;
                continue;
            }
            
            auto session = std::make_shared<GameSession>(map, tick_period_, 
                                                          game_.GetLootGeneratorConfig(), ioc);
            
            // Восстанавливаем lost objects
            for (const auto& lost_obj_ser : session_ser.GetLostObjectsSerialize()) {
                session->AddLostObject(lost_obj_ser.Restore());
            }
            
            // Восстанавливаем игроков этой сессии
            for (const auto& player_ser : session_ser.GetPlayersSerialize()) {
                auto player = std::make_shared<Player>(player_ser.Restore());
                auto dog = std::make_shared<model::Dog>(player_ser.RestoreDog());
                auto token = player_ser.RestoreToken();
                
                // Восстанавливаем связи
                player->SetDog(dog);
                session->AddDog(dog);
                
                RestorePlayer(token, player, session);
            }
            
            AddGameSession(session);
            session->Run();
        }
        
        // Восстанавливаем игроков без сессий (если есть)
        for (const auto& player_ser : state.players) {
            // Проверяем, не был ли уже восстановлен этот игрок
            auto token = player_ser.RestoreToken();
            if (auth_token_to_session_index_.find(token) == auth_token_to_session_index_.end()) {
                auto player = std::make_shared<Player>(player_ser.Restore());
                auto dog = std::make_shared<model::Dog>(player_ser.RestoreDog());
                
                player->SetDog(dog);
                player_tokens_.AddTokenPlayerPair(token, player);
                players_.push_back(player);
                player_id_to_token_[player->GetId()] = token;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Exception while restoring state: " << e.what();
        return false;
    }
};

void Application::RestorePlayer(const authentication::Token& token, std::shared_ptr<Player> player, 
                                 std::shared_ptr<GameSession> session) {
    // Добавляем в список всех игроков
    players_.push_back(player);
    
    // Сохраняем связь токен -> игрок
    player_tokens_.AddTokenPlayerPair(token, player);
    
    // Сохраняем связь токен -> сессия
    auth_token_to_session_index_[token] = session;
    
    // Сохраняем связь сессия -> {токен -> игрок}
    game_session_to_token_player_pair_[session][token] = player;
    
    // Сохраняем связь игрок -> сессия в session_id_to_players_
    session_id_to_players_[session->GetId()].push_back(player);
    
    // Сохраняем обратную связь ID игрока -> токен
    player_id_to_token_[player->GetId()] = token;
};

}