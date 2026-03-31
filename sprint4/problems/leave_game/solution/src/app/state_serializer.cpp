bool StateSerializer::LoadState(net::io_context& ioc) {
    try {
        if (!std::filesystem::exists(state_file_)) {
            BOOST_LOG_TRIVIAL(info) << "State file not found: " << state_file_.string();
            return false;
        }
        
        if (std::filesystem::file_size(state_file_) == 0) {
            BOOST_LOG_TRIVIAL(warning) << "State file is empty: " << state_file_.string();
            return false;
        }
        
        BOOST_LOG_TRIVIAL(info) << "=== LOADING GAME STATE ===";
        BOOST_LOG_TRIVIAL(info) << "Loading game state from " << state_file_.string();
        
        GameState game_state;
        std::ifstream ifs(state_file_.string());
        if (!ifs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to open state file for reading: " << state_file_.string();
            return false;
        }
        
        {
            boost::archive::text_iarchive ia(ifs);
            ia >> game_state;
        }
        ifs.close();
        
        BOOST_LOG_TRIVIAL(info) << "Loaded " << game_state.data.sessions.size() 
                               << " sessions from state file";
        
        // Восстанавливаем счетчики ID
        size_t max_player_id_restored = static_cast<size_t>(game_state.max_player_id);
        size_t max_dog_id_restored = static_cast<size_t>(game_state.max_dog_id);
        size_t max_loot_id_restored = static_cast<size_t>(game_state.max_loot_id);
        
        Player::ResetMaxId(max_player_id_restored);
        model::Dog::ResetMaxId(max_dog_id_restored);
        model::LostObject::ResetMaxId(max_loot_id_restored);
        
        BOOST_LOG_TRIVIAL(info) << "Restored counters: player_max=" << max_player_id_restored
                                << ", dog_max=" << max_dog_id_restored
                                << ", loot_max=" << max_loot_id_restored;
        
        // Восстанавливаем сессии
        for (const auto& session_ser : game_state.data.sessions) {
            auto map_id = session_ser.RestoreMapId();
            BOOST_LOG_TRIVIAL(info) << "Restoring session for map: " << *map_id;
            
            auto map = app_.FindMap(map_id);
            if (!map) {
                BOOST_LOG_TRIVIAL(error) << "Map not found for restored session: " << *map_id;
                continue;
            }
            
            auto session = std::make_shared<GameSession>(
                map, 
                app_.GetTickPeriod(), 
                app_.GetLootGeneratorConfig(), 
                ioc,
                app_.GetDogRetirementTime()
            );
            
            // Устанавливаем callback для уведомления о retirement
            session->SetRetirementCallback(
                [this](const authentication::Token& token, size_t player_id, int64_t play_time_ms) {
                    auto player = app_.FindPlayerById(player_id);
                    if (player) {
                        app_.RemovePlayerAndSaveRecord(token, player, play_time_ms);
                    }
                }
            );
            
            // Устанавливаем finder для поиска токена по ID игрока
            session->SetTokenFinder(
                [this](size_t player_id) -> std::optional<authentication::Token> {
                    return app_.FindTokenByPlayer(player_id);
                }
            );
            
            // Восстанавливаем потерянные объекты
            size_t lost_objects_restored = 0;
            for (const auto& lost_obj_ser : session_ser.GetLostObjectsSerialize()) {
                auto lost_obj = std::make_shared<model::LostObject>(lost_obj_ser.Restore());
                session->AddLostObject(lost_obj);
                lost_objects_restored++;
                if (*lost_obj->GetId() >= model::LostObject::GetMaxId()) {
                    model::LostObject::ResetMaxId(*lost_obj->GetId() + 1);
                }
            }
            BOOST_LOG_TRIVIAL(info) << "  Restored " << lost_objects_restored << " lost objects";
            
            // Восстанавливаем игроков
            const auto& players_ser = session_ser.GetPlayersSerialize();
            BOOST_LOG_TRIVIAL(info) << "  Found " << players_ser.size() << " players to restore";
            
            for (const auto& player_ser : players_ser) {
                auto player = std::make_shared<Player>(player_ser.Restore());
                player->SetJoinTime(player_ser.GetJoinTime());  // <-- ЭТА СТРОКА ВАЖНА
                
                auto dog = std::make_shared<model::Dog>(player_ser.RestoreDog());
                
                player->SetDog(dog);
                session->AddDog(dog);
                
                BOOST_LOG_TRIVIAL(debug) << "    Restored join_time: " 
                                         << player->GetJoinTime().time_since_epoch().count();
                
                if (*player->GetId() >= Player::GetMaxId()) {
                    Player::ResetMaxId(*player->GetId() + 1);
                    BOOST_LOG_TRIVIAL(debug) << "    Updated player max_id to " << Player::GetMaxId();
                }
                if (*dog->GetId() >= model::Dog::GetMaxId()) {
                    model::Dog::ResetMaxId(*dog->GetId() + 1);
                    BOOST_LOG_TRIVIAL(debug) << "    Updated dog max_id to " << model::Dog::GetMaxId();
                }
                
                auto token = player_ser.RestoreToken();
                
                BOOST_LOG_TRIVIAL(info) << "    Restoring player: " << player->GetName() 
                                        << " id=" << *player->GetId() 
                                        << " token=" << *token;
                
                app_.RestorePlayer(token, player, session);
            }
            
            app_.AddGameSession(session);
            session->Run();
            
            BOOST_LOG_TRIVIAL(info) << "  Session for map " << *map_id << " restored and started";
        }
        
        BOOST_LOG_TRIVIAL(info) << "Game state loaded successfully from " << state_file_.string();
        return true;
        
    } catch (const boost::archive::archive_exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Archive error while loading game state: " << e.what();
        return false;
    } catch (const std::ifstream::failure& e) {
        BOOST_LOG_TRIVIAL(error) << "File I/O error while loading game state: " << e.what();
        return false;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to load game state: " << e.what();
        return false;
    }
}