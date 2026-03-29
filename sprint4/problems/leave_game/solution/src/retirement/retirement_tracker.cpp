#include "retirement/retirement_tracker.h"
#include <algorithm>

namespace retirement {

void RetirementTracker::UpdateActivity(uint64_t dog_id, const TimePoint& now) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = dogs_.find(dog_id);
    if (it == dogs_.end()) {
        // Новая собака: устанавливаем время активности и начало бездействия
        DogInactivityData data;
        data.last_activity_time = now;
        data.inactivity_start_time = now;
        dogs_[dog_id] = data;
    } else {
        // Обновляем время последней активности
        it->second.last_activity_time = now;
        // Сбрасываем время начала бездействия (собака двигалась)
        it->second.inactivity_start_time = now;
    }
}

bool RetirementTracker::IsRetired(uint64_t dog_id, const TimePoint& now, Duration retirement_timeout) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = dogs_.find(dog_id);
    if (it == dogs_.end()) {
        return false;
    }
    
    Duration inactive_duration = std::chrono::duration_cast<Duration>(
        now - it->second.last_activity_time
    );
    
    return inactive_duration >= retirement_timeout;
}

RetirementTracker::Duration RetirementTracker::GetInactivityDuration(uint64_t dog_id, const TimePoint& now) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = dogs_.find(dog_id);
    if (it == dogs_.end()) {
        return Duration{0};
    }
    
    return std::chrono::duration_cast<Duration>(
        now - it->second.last_activity_time
    );
}

void RetirementTracker::RemoveDog(uint64_t dog_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    dogs_.erase(dog_id);
}

bool RetirementTracker::HasDog(uint64_t dog_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dogs_.find(dog_id) != dogs_.end();
}

RetirementTracker::TimePoint RetirementTracker::GetInactivityStartTime(uint64_t dog_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = dogs_.find(dog_id);
    if (it == dogs_.end()) {
        return TimePoint{};
    }
    
    return it->second.inactivity_start_time;
}

} // namespace retirement