#pragma once
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace retirement {

class RetirementTracker {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::milliseconds;
    
    RetirementTracker() = default;
    
    void UpdateActivity(uint64_t dog_id, const TimePoint& now);
    bool IsRetired(uint64_t dog_id, const TimePoint& now, Duration retirement_timeout) const;
    Duration GetInactivityDuration(uint64_t dog_id, const TimePoint& now) const;
    void RemoveDog(uint64_t dog_id);
    bool HasDog(uint64_t dog_id) const;
    TimePoint GetInactivityStartTime(uint64_t dog_id) const;
    TimePoint GetLastActivityTime(uint64_t dog_id) const;
    
private:
    struct DogInactivityData {
        TimePoint last_activity_time;
        TimePoint inactivity_start_time;
    };
    
    std::unordered_map<uint64_t, DogInactivityData> dogs_;
    mutable std::mutex mutex_;
};

}