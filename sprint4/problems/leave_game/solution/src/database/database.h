#pragma once
#include "database/player_record.h"
#include "database/connection_pool.h"
#include <memory>
#include <vector>
#include <string>

namespace database {

class Database {
public:
    static void Init(std::shared_ptr<ConnectionPool> pool);
    static void SaveRecord(std::shared_ptr<ConnectionPool> pool, const PlayerRecord& record);
    static std::vector<PlayerRecord> GetRecords(std::shared_ptr<ConnectionPool> pool, 
                                                 int start, 
                                                 int maxItems);
    static bool TableExists(std::shared_ptr<ConnectionPool> pool, const std::string& table_name);
    static void CreateTableIfNotExists(std::shared_ptr<ConnectionPool> pool);
    static void CleanupOldRecords(std::shared_ptr<ConnectionPool> pool, int maxRecords = 10000);
};

}