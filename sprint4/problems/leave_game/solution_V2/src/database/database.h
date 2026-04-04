#pragma once
#include "database/player_record.h"
#include "database/connection_pool.h"
#include <memory>
#include <vector>
#include <string>

namespace database {

class Database {
public:
    // Инициализация: создание таблицы retired_players
    static void Init(std::shared_ptr<ConnectionPool> pool);
    
    // Сохранение записи об ушедшем игроке
    static void SaveRecord(std::shared_ptr<ConnectionPool> pool, const PlayerRecord& record);
    
    // Получение списка рекордов с пагинацией
    static std::vector<PlayerRecord> GetRecords(std::shared_ptr<ConnectionPool> pool, 
                                                 int start, 
                                                 int maxItems);
    
    // Проверка существования таблицы
    static bool TableExists(std::shared_ptr<ConnectionPool> pool, const std::string& table_name);
    
    // Создание таблицы (если не существует)
    static void CreateTableIfNotExists(std::shared_ptr<ConnectionPool> pool);
};

} // namespace database