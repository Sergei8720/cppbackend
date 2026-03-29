#include "database/database.h"
#include <pqxx/pqxx>
#include <stdexcept>
#include <string>

namespace database {

void Database::Init(std::shared_ptr<ConnectionPool> pool) {
    CreateTableIfNotExists(pool);
}

bool Database::TableExists(std::shared_ptr<ConnectionPool> pool, const std::string& table_name) {
    auto conn = pool->GetConnection();
    pqxx::work work(*conn);
    
    std::string query = "SELECT EXISTS ("
                        "SELECT 1 FROM information_schema.tables "
                        "WHERE table_name = '" + table_name + "'"
                        ")";
    
    pqxx::result result = work.exec(query);
    work.commit();
    
    return result[0][0].as<bool>();
}

void Database::CreateTableIfNotExists(std::shared_ptr<ConnectionPool> pool) {
    auto conn = pool->GetConnection();
    
    // Создаём таблицу retired_players
    pqxx::work work(*conn);
    work.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id UUID PRIMARY KEY,
            name VARCHAR(100) NOT NULL,
            score BIGINT NOT NULL,
            play_time_ms BIGINT NOT NULL
        )
    )");
    
    // Создаём индексы для быстрой сортировки
    work.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_retired_players_score 
        ON retired_players(score DESC, play_time_ms ASC, name ASC)
    )");
    
    work.commit();
}

void Database::SaveRecord(std::shared_ptr<ConnectionPool> pool, const PlayerRecord& record) {
    auto conn = pool->GetConnection();
    pqxx::work work(*conn);
    
    // UPSERT: вставляем или обновляем при конфликте (по UUID)
    work.exec_params(
        "INSERT INTO retired_players (id, name, score, play_time_ms) "
        "VALUES ($1, $2, $3, $4) "
        "ON CONFLICT (id) DO UPDATE SET "
        "name = EXCLUDED.name, "
        "score = EXCLUDED.score, "
        "play_time_ms = EXCLUDED.play_time_ms",
        record.id_uuid, record.name, record.score, record.play_time_ms
    );
    
    work.commit();
}

std::vector<PlayerRecord> Database::GetRecords(std::shared_ptr<ConnectionPool> pool, 
                                                int start, 
                                                int maxItems) {
    if (maxItems > 100) {
        throw std::runtime_error("maxItems cannot exceed 100");
    }
    
    auto conn = pool->GetConnection();
    pqxx::read_transaction tx(*conn);
    
    std::string query = "SELECT id, name, score, play_time_ms "
                        "FROM retired_players "
                        "ORDER BY score DESC, play_time_ms ASC, name ASC "
                        "OFFSET " + std::to_string(start) + " "
                        "LIMIT " + std::to_string(maxItems);
    
    pqxx::result result = tx.exec(query);
    
    std::vector<PlayerRecord> records;
    records.reserve(result.size());
    
    for (const auto& row : result) {
        records.emplace_back(
            row[0].as<std::string>(),   // id (UUID)
            row[1].as<std::string>(),   // name
            row[2].as<int64_t>(),       // score
            row[3].as<int64_t>()        // play_time_ms
        );
    }
    
    return records;
}

} // namespace database