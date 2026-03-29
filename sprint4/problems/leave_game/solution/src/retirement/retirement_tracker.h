#pragma once
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace retirement {

// Трекер бездействия собак
class RetirementTracker {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::milliseconds;
    
    RetirementTracker() = default;
    
    // Обновить время последней активности собаки
    void UpdateActivity(uint64_t dog_id, const TimePoint& now);
    
    // Проверить, не пора ли отправить собаку на покой
    bool IsRetired(uint64_t dog_id, const TimePoint& now, Duration retirement_timeout) const;
    
    // Получить время бездействия собаки (в миллисекундах)
    Duration GetInactivityDuration(uint64_t dog_id, const TimePoint& now) const;
    
    // Удалить собаку из трекера (при выходе из игры)
    void RemoveDog(uint64_t dog_id);
    
    // Проверить, отслеживается ли собака
    bool HasDog(uint64_t dog_id) const;
    
    // Получить время начала бездействия (для подсчёта play_time)
    TimePoint GetInactivityStartTime(uint64_t dog_id) const;
    
private:
    struct DogInactivityData {
        TimePoint last_activity_time;      // Время последнего движения
        TimePoint inactivity_start_time;   // Время начала текущего периода бездействия
    };
    
    std::unordered_map<uint64_t, DogInactivityData> dogs_;
    mutable std::mutex mutex_;
};

} // namespace retirement