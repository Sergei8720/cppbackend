#pragma once
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <pqxx/pqxx>

namespace database {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;
    
    class ConnectionWrapper {
    public:
        ConnectionWrapper(ConnectionPtr&& conn, ConnectionPool& pool) noexcept
            : conn_(std::move(conn))
            , pool_(&pool) {
        }
        
        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
        
        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;
        
        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        
        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }
        
        ~ConnectionWrapper() {
            if (conn_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }
        
    private:
        ConnectionPtr conn_;
        ConnectionPool* pool_;
    };
    
    // ConnectionFactory - функтор, возвращающий std::shared_ptr<pqxx::connection>
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }
    
    ConnectionWrapper GetConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] {
            return used_connections_ < pool_.size();
        });
        return {std::move(pool_[used_connections_++]), *this};
    }
    
    size_t Size() const {
        return pool_.size();
    }
    
private:
    void ReturnConnection(ConnectionPtr&& conn) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // assert(used_connections_ != 0);
            pool_[--used_connections_] = std::move(conn);
        }
        cond_var_.notify_one();
    }
    
    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};

} // namespace database