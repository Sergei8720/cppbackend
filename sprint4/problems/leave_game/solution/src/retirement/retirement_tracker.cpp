#include "retirement/retirement_tracker.h"
#include "logger.h"
#include <algorithm>

namespace retirement {

void RetirementTracker::UpdateActivity(uint64_t dog_id, const TimePoint& now) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = dogs_.find(dog_id);
    if (it == dogs_.end()) {
        DogInactivityData data;
        data.last_activity_time = now;
        data.inactivity_start_time = now;
        dogs_[dog_id] = data;
        BOOST_LOG_TRIVIAL(debug) << "New dog " << dog_id << " added to tracker";
    } else {
        it->second.last_activity_time = now;
        it->second.inactivity_start_time = now;
        BOOST_LOG_TRIVIAL(debug) << "Dog " << dog_id << " activity updated";
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
    
    bool retired = inactive_duration >= retirement_timeout;
    
    if (retired) {
        BOOST_LOG_TRIVIAL(debug) << "Dog " << dog_id << " is retired (inactive for " 
                                 << inactive_duration.count() << "ms, timeout=" 
                                 << retirement_timeout.count() << "ms)";
    }
    
    return retired;
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
    auto erased = dogs_.erase(dog_id);
    if (erased > 0) {
        BOOST_LOG_TRIVIAL(debug) << "Dog " << dog_id << " removed from tracker";
    }
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