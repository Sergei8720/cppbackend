#pragma once
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <pqxx/pqxx>

namespace database {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;
    
    class ConnectionWrapper {
    public:
        ConnectionWrapper(ConnectionPtr conn, ConnectionPool& pool) noexcept
            : conn_(std::move(conn))
            , pool_(&pool) {
        }
        
        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
        
        ConnectionWrapper(ConnectionWrapper&& other) noexcept
            : conn_(std::move(other.conn_))
            , pool_(other.pool_) {
            other.pool_ = nullptr;
        }
        
        ConnectionWrapper& operator=(ConnectionWrapper&& other) noexcept {
            if (this != &other) {
                if (conn_ && pool_) {
                    pool_->ReturnConnection(std::move(conn_));
                }
                conn_ = std::move(other.conn_);
                pool_ = other.pool_;
                other.pool_ = nullptr;
            }
            return *this;
        }
        
        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        
        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }
        
        ~ConnectionWrapper() {
            if (conn_ && pool_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }
        
    private:
        ConnectionPtr conn_;
        ConnectionPool* pool_;
    };
    
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory)
        : capacity_(capacity)
        , used_connections_(0)
    {
        if (capacity == 0) {
            throw std::invalid_argument("ConnectionPool capacity cannot be 0");
        }
        
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            auto conn = connection_factory();
            if (!conn) {
                throw std::runtime_error("Failed to create database connection: connection is null");
            }
            if (!conn->is_open()) {
                throw std::runtime_error("Failed to create database connection: connection is not open");
            }
            pool_.push_back(conn);
            free_queue_.push(i);
        }
    }
    
    ConnectionWrapper GetConnection() {
        return GetConnection(std::chrono::seconds(5));
    }
    
    ConnectionWrapper GetConnection(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto current_id = std::this_thread::get_id();
        if (current_id == current_thread_id_ && owned_connections_ > 0) {
            throw std::runtime_error("Thread already owns a connection from this pool (deadlock prevented)");
        }
        
        bool success = cond_var_.wait_for(lock, timeout, [this] {
            return !free_queue_.empty();
        });
        
        if (!success) {
            throw std::runtime_error("Timeout waiting for free connection");
        }
        
        size_t index = free_queue_.front();
        free_queue_.pop();
        used_connections_++;
        
        current_thread_id_ = std::this_thread::get_id();
        owned_connections_++;
        
        return ConnectionWrapper(pool_[index], *this);
    }
    
    size_t Size() const {
        return capacity_;
    }
    
    size_t AvailableCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return free_queue_.size();
    }
    
private:
    void ReturnConnection(ConnectionPtr conn) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            for (size_t i = 0; i < capacity_; ++i) {
                if (pool_[i] == conn) {
                    free_queue_.push(i);
                    break;
                }
            }
            
            used_connections_--;
            owned_connections_--;
            
            if (owned_connections_ == 0) {
                current_thread_id_ = std::thread::id();
            }
        }
        cond_var_.notify_one();
    }
    
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    std::queue<size_t> free_queue_;
    size_t capacity_;
    size_t used_connections_;
    
    std::thread::id current_thread_id_;
    size_t owned_connections_ = 0;
};

}