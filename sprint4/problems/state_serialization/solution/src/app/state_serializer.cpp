#include "state_serializer.h"
#include "game_session_serialization.h"
#include "player_serialization.h"
#include "logger.h"
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace app {

using namespace std::literals;

struct GameState {
    std::vector<game_data_ser::GameSessionSerialization> sessions;
    std::vector<game_data_ser::PlayerSerialization> players;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar& sessions;
        ar& players;
    }
};

StateSerializer::StateSerializer(Application& application,
                                 const std::filesystem::path& state_file,
                                 std::chrono::milliseconds save_period)
    : app_(application)
    , state_file_(state_file)
    , save_period_(save_period) {
}

void StateSerializer::SaveState() {
    try {
        GameState game_state;
        
        // Сохраняем все игровые сессии
        for (const auto& session : app_.GetSessions()) {
            // Собираем токены игроков для этой сессии
            std::unordered_map<authentication::Token, std::shared_ptr<Player>, authentication::TokenHasher> token_to_player;
            for (const auto& player : session->GetPlayers()) {
                auto token = app_.FindTokenByPlayer(player->GetId());
                if (token.has_value()) {
                    token_to_player[token.value()] = player;
                }
            }
            
            game_state.sessions.emplace_back(*session, token_to_player);
        }
        
        // Сохраняем всех игроков (на случай, если есть игроки без сессий)
        for (const auto& player : app_.GetAllPlayers()) {
            auto token = app_.FindTokenByPlayer(player->GetId());
            if (token.has_value()) {
                game_state.players.emplace_back(*player, token.value());
            }
        }
        
        // Записываем в файл
        std::ofstream ofs(state_file_.string());
        if (ofs.is_open()) {
            boost::archive::text_oarchive oa(ofs);
            oa << game_state;
            BOOST_LOG_TRIVIAL(info) << "Game state saved to " << state_file_.string();
        } else {
            BOOST_LOG_TRIVIAL(error) << "Failed to open state file for writing: " << state_file_.string();
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to save game state: " << e.what();
    }
}

bool StateSerializer::LoadState(net::io_context& ioc) {
    try {
        if (!std::filesystem::exists(state_file_)) {
            BOOST_LOG_TRIVIAL(info) << "State file not found: " << state_file_.string();
            return false;
        }
        
        GameState game_state;
        std::ifstream ifs(state_file_.string());
        if (ifs.is_open()) {
            boost::archive::text_iarchive ia(ifs);
            ia >> game_state;
            
            // Восстанавливаем сессии
            for (const auto& session_ser : game_state.sessions) {
                auto map_id = session_ser.RestoreMapId();
                auto map = app_.FindMap(map_id);
                if (!map) {
                    BOOST_LOG_TRIVIAL(error) << "Map not found for restored session: " << *map_id;
                    continue;
                }
                
                auto session = std::make_shared<GameSession>(map, app_.GetTickPeriod(), 
                                                            app_.GetLootGeneratorConfig(), ioc);
                
                // Восстанавливаем lost objects (исправлено: создаём shared_ptr)
                for (const auto& lost_obj_ser : session_ser.GetLostObjectsSerialize()) {
                    session->AddLostObject(std::make_shared<model::LostObject>(lost_obj_ser.Restore()));
                }
                
                // Восстанавливаем игроков
                for (const auto& player_ser : session_ser.GetPlayersSerialize()) {
                    auto player = std::make_shared<Player>(player_ser.Restore());
                    auto dog = std::make_shared<model::Dog>(player_ser.RestoreDog());
                    
                    // Восстанавливаем связь player-dog
                    player->SetDog(dog);
                    
                    // Добавляем в сессию
                    session->AddDog(dog);
                    
                    // Восстанавливаем токен
                    auto token = player_ser.RestoreToken();
                    app_.RestorePlayer(token, player, session);
                }
                
                app_.AddGameSession(session);
            }
            
            BOOST_LOG_TRIVIAL(info) << "Game state loaded from " << state_file_.string();
            return true;
        } else {
            BOOST_LOG_TRIVIAL(error) << "Failed to open state file for reading: " << state_file_.string();
            return false;
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to load game state: " << e.what();
        return false;
    }
}

void StateSerializer::StartPeriodicSaving(net::io_context& ioc) {
    if (save_period_.count() <= 0) {
        return;
    }
    
    auto strand = std::make_shared<net::strand<net::io_context::executor_type>>(net::make_strand(ioc));
    
    save_ticker_ = std::make_shared<time_m::Ticker>(
        strand,
        save_period_,
        [this](const std::chrono::milliseconds&) {
            this->SaveState();  // используем сохранённую ссылку app_
        }
    );
    save_ticker_->Start();
}

// Если метод OnSaveTimer не используется, его можно удалить

} // namespace app