#ifndef MY_LOGGER_H
#define MY_LOGGER_H

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <algorithm>
#include <filesystem>

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    template <typename... Ts>
    void Log(const Ts&... args) {
        std::shared_lock lock(mutex_);
        log_stream_ << GetTimestamp() << ": ";
        ((log_stream_ << args), ...);
        log_stream_ << std::endl;
    }

    void SetTimestamp(std::chrono::system_clock::time_point timestamp) {
        std::lock_guard lock(mutex_);
        manual_timestamp_ = timestamp;
        ReopenLogFile();
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

private:
    static constexpr const char* LOG_DIRECTORY = "/var/log/";
    static constexpr const char* LOG_EXTENSION = ".log";
    static constexpr const char* LOG_BASE_NAME = "sample_log_";

    Logger() {
        ReopenLogFile();
    }

    ~Logger() {
        if (log_stream_.is_open()) {
            log_stream_.close();
        }
    }

    std::chrono::system_clock::time_point GetCurrentTime() const {
        if (manual_timestamp_) {
            return *manual_timestamp_;
        }
        return std::chrono::system_clock::now();
    }

    std::string GetTimestamp() const {
        const auto now = GetCurrentTime();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream stream;
        stream << std::put_time(std::localtime(&time), "%F %T");
        return stream.str();
    }

    std::string GetDateForFilename() const {
        const auto now = GetCurrentTime();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream stream;
        stream << std::put_time(std::localtime(&time), "%F");
        std::string date = stream.str();
        std::replace(date.begin(), date.end(), '-', '_');
        return date;
    }

    std::string GenerateLogFilename() const {
        return std::string(LOG_DIRECTORY) + 
               std::string(LOG_BASE_NAME) + 
               GetDateForFilename() + 
               std::string(LOG_EXTENSION);
    }

    void ReopenLogFile() {
        if (log_stream_.is_open()) {
            log_stream_.close();
        }
        
        std::string filename = GenerateLogFilename();
        log_stream_.open(filename, std::ios::app);
        
        if (!log_stream_.is_open()) {
            std::filesystem::create_directories(LOG_DIRECTORY);
            log_stream_.open(filename, std::ios::app);
        }
    }

    std::optional<std::chrono::system_clock::time_point> manual_timestamp_;
    mutable std::shared_mutex mutex_;
    std::ofstream log_stream_;
};

#endif  // MY_LOGGER_H