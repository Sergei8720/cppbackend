#include "database/database.h"
#include "logger.h"
#include <pqxx/pqxx>
#include <stdexcept>
#include <string>

namespace database {

void Database::Init(std::shared_ptr<ConnectionPool> pool) {
    if (!pool) {
        BOOST_LOG_TRIVIAL(error) << "Database::Init: connection pool is null";
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "Database::Init: creating tables...";
    CreateTableIfNotExists(pool);
    BOOST_LOG_TRIVIAL(info) << "Database::Init: completed";
}

bool Database::TableExists(std::shared_ptr<ConnectionPool> pool, const std::string& table_name) {
    if (!pool) {
        BOOST_LOG_TRIVIAL(error) << "TableExists: pool is null";
        return false;
    }
    
    try {
        auto conn = pool->GetConnection();
        pqxx::work work(*conn);
        
        // Используем параметризованный запрос для безопасности
        std::string query = "SELECT EXISTS ("
                            "SELECT 1 FROM information_schema.tables "
                            "WHERE table_name = $1"
                            ")";
        
        pqxx::result result = work.exec_params(query, table_name);
        work.commit();
        
        return result[0][0].as<bool>();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "TableExists failed: " << e.what();
        return false;
    }
}

void Database::CreateTableIfNotExists(std::shared_ptr<ConnectionPool> pool) {
    if (!pool) {
        BOOST_LOG_TRIVIAL(error) << "CreateTableIfNotExists: pool is null";
        return;
    }
    
    try {
        auto conn = pool->GetConnection();
        pqxx::work work(*conn);
        
        // Создаем таблицу
        work.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
                id BIGINT PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                score BIGINT NOT NULL,
                play_time_ms BIGINT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )");
        
        // Создаем составной индекс для быстрой сортировки по score DESC, play_time_ms ASC, name ASC
        work.exec(R"(
            CREATE INDEX IF NOT EXISTS idx_retired_players_score_time_name 
            ON retired_players(score DESC, play_time_ms ASC, name ASC)
        )");
        
        // Дополнительный индекс для быстрой очистки старых записей
        work.exec(R"(
            CREATE INDEX IF NOT EXISTS idx_retired_players_created_at 
            ON retired_players(created_at DESC)
        )");
        
        work.commit();
        BOOST_LOG_TRIVIAL(info) << "Table 'retired_players' is ready";
        
        // Проверяем, что таблица создалась
        if (TableExists(pool, "retired_players")) {
            BOOST_LOG_TRIVIAL(debug) << "Verified: table 'retired_players' exists";
        } else {
            BOOST_LOG_TRIVIAL(warning) << "Table 'retired_players' was not created";
        }
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to create table: " << e.what();
        throw; // Пробрасываем исключение дальше, так как без таблицы работа невозможна
    }
}

void Database::SaveRecord(std::shared_ptr<ConnectionPool> pool, const PlayerRecord& record) {
    if (!pool) {
        BOOST_LOG_TRIVIAL(error) << "SaveRecord: connection pool is null, record not saved";
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "SaveRecord called: id=" << record.id_uuid 
                            << ", name=" << record.name 
                            << ", score=" << record.score 
                            << ", play_time_ms=" << record.play_time_ms;
    
    const int max_retries = 3;
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        try {
            auto conn = pool->GetConnection();
            
            if (!conn->is_open()) {
                throw std::runtime_error("Database connection is not open");
            }
            
            pqxx::work work(*conn);
            
            work.exec_params(
                "INSERT INTO retired_players (id, name, score, play_time_ms) "
                "VALUES ($1, $2, $3, $4) "
                "ON CONFLICT (id) DO UPDATE SET "
                "name = EXCLUDED.name, "
                "score = EXCLUDED.score, "
                "play_time_ms = EXCLUDED.play_time_ms, "
                "created_at = CURRENT_TIMESTAMP",
                record.id_uuid, record.name, record.score, record.play_time_ms
            );
            
            work.commit();
            BOOST_LOG_TRIVIAL(info) << "Record saved successfully: " << record.name;
            return; // Успешно сохранили, выходим
            
        } catch (const pqxx::broken_connection& e) {
            BOOST_LOG_TRIVIAL(warning) << "Broken connection, retry " << attempt + 1 
                                       << "/" << max_retries << ": " << e.what();
            if (attempt == max_retries - 1) {
                BOOST_LOG_TRIVIAL(error) << "Failed to save record after " << max_retries << " attempts";
            }
            // Небольшая задержка перед повторной попыткой
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
            
        } catch (const pqxx::sql_error& e) {
            BOOST_LOG_TRIVIAL(error) << "SQL error while saving record: " << e.what();
            BOOST_LOG_TRIVIAL(error) << "Query was: " << e.query();
            break; // SQL ошибка не исправится повторением, выходим
            
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Failed to save record: " << e.what();
            break;
        }
    }
}

std::vector<PlayerRecord> Database::GetRecords(std::shared_ptr<ConnectionPool> pool, 
                                                int start, 
                                                int maxItems) {
    if (!pool) {
        BOOST_LOG_TRIVIAL(warning) << "GetRecords: connection pool is null, returning empty list";
        return {};
    }
    
    // Валидация параметров
    if (maxItems > 100) {
        BOOST_LOG_TRIVIAL(warning) << "GetRecords: maxItems=" << maxItems << " exceeds 100, using 100";
        maxItems = 100;
    }
    
    if (maxItems <= 0) {
        BOOST_LOG_TRIVIAL(warning) << "GetRecords: maxItems=" << maxItems << " is invalid, using 100";
        maxItems = 100;
    }
    
    if (start < 0) {
        BOOST_LOG_TRIVIAL(warning) << "GetRecords: start=" << start << " is negative, using 0";
        start = 0;
    }
    
    try {
        auto conn = pool->GetConnection();
        
        if (!conn->is_open()) {
            BOOST_LOG_TRIVIAL(error) << "GetRecords: database connection is not open";
            return {};
        }
        
        pqxx::read_transaction tx(*conn);
        
        // Используем подготовленный запрос для оптимальной производительности
        static const std::string query = 
            "SELECT id, name, score, play_time_ms "
            "FROM retired_players "
            "ORDER BY score DESC, play_time_ms ASC, name ASC "
            "OFFSET $1 LIMIT $2";
        
        pqxx::result result = tx.exec_params(query, start, maxItems);
        
        std::vector<PlayerRecord> records;
        records.reserve(result.size());
        
        for (const auto& row : result) {
            records.emplace_back(
                row[0].as<int64_t>(),
                row[1].as<std::string>(),
                row[2].as<int64_t>(),
                row[3].as<int64_t>()
            );
        }
        
        BOOST_LOG_TRIVIAL(debug) << "GetRecords returned " << records.size() 
                                 << " records (start=" << start << ", maxItems=" << maxItems << ")";
        return records;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to get records: " << e.what();
        return {};
    }
}

void Database::CleanupOldRecords(std::shared_ptr<ConnectionPool> pool, int maxRecords) {
    if (!pool) {
        BOOST_LOG_TRIVIAL(error) << "CleanupOldRecords: pool is null";
        return;
    }
    
    try {
        auto conn = pool->GetConnection();
        pqxx::work work(*conn);
        
        // Удаляем старые записи, оставляя только maxRecords самых новых
        work.exec_params(
            "DELETE FROM retired_players "
            "WHERE id IN ("
            "  SELECT id FROM retired_players "
            "  ORDER BY created_at DESC "
            "  OFFSET $1"
            ")",
            maxRecords
        );
        
        work.commit();
        BOOST_LOG_TRIVIAL(info) << "Cleaned up old records, kept last " << maxRecords;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to cleanup old records: " << e.what();
    }
}

} // namespace database