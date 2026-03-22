#include "roadmap.h"
#include "random_generators.h"
#include "model_invariants.h"
#include "logger.h"

#include <cmath>
#include <stdint.h>
#include <set>

namespace model {

const int SCALE_FACTOR_OF_CELL = 20;

void Roadmap::AddCoordinatesToMatrixMap(int64_t x, int64_t y, size_t road_index) {
    matrix_map_[x][y].insert(road_index);
}

void Roadmap::AddRoad(const Road& road) {
    const auto SCALLED_OFFSET = static_cast<int64_t>(OFFSET * SCALE_FACTOR_OF_CELL);
    auto index = roads_.size();
    roads_.emplace_back(road);
    
    if(road.IsHorizontal()) {
        auto start = static_cast<int64_t>((road.GetStart().x < road.GetEnd().x) ? road.GetStart().x : road.GetEnd().x);
        auto end = static_cast<int64_t>((road.GetStart().x < road.GetEnd().x) ? road.GetEnd().x : road.GetStart().x);
        start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
        end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
        auto y = road.GetStart().y * SCALE_FACTOR_OF_CELL;
        for(auto x = start; x <= end; ++x) {
            for(auto i = -(SCALLED_OFFSET); i <= SCALLED_OFFSET; ++i) {
                AddCoordinatesToMatrixMap(x, y + i, index);
            }
        }
    } else {
        auto start = static_cast<int64_t>((road.GetStart().y < road.GetEnd().y) ? road.GetStart().y : road.GetEnd().y);
        auto end = static_cast<int64_t>((road.GetStart().y < road.GetEnd().y) ? road.GetEnd().y : road.GetStart().y);
        start = start * SCALE_FACTOR_OF_CELL - SCALLED_OFFSET;
        end = end * SCALE_FACTOR_OF_CELL + SCALLED_OFFSET;
        auto x = road.GetStart().x * SCALE_FACTOR_OF_CELL;
        for(auto y = start; y <= end; ++y) {
            for(auto i = -SCALLED_OFFSET; i <= SCALLED_OFFSET; ++i) {
                AddCoordinatesToMatrixMap(x + i, y, index);
            }
        }
    }
};

const Roadmap::Roads& Roadmap::GetRoads() const noexcept {
    return roads_;
};

bool Roadmap::IsValidPositionOnRoad(const Road& road, const geom::Point2D& position) {
    double start_x, end_x, start_y, end_y;
    if(road.IsHorizontal()) {
        start_x = (road.GetStart().x < road.GetEnd().x) ? (road.GetStart().x) : (road.GetEnd().x);
        end_x = (road.GetStart().x < road.GetEnd().x) ? (road.GetEnd().x) : (road.GetStart().x);
        start_y = road.GetStart().y - OFFSET;
        end_y = road.GetStart().y + OFFSET;

        start_x -= OFFSET;
        end_x += OFFSET;
    } else {
        start_y = (road.GetStart().y < road.GetEnd().y) ? (road.GetStart().y) : (road.GetEnd().y);
        end_y = (road.GetStart().y < road.GetEnd().y) ? (road.GetEnd().y) : (road.GetStart().y);
        start_x = road.GetStart().x - OFFSET;
        end_x = road.GetStart().x + OFFSET;

        start_y -= OFFSET;
        end_y += OFFSET;
    }
    return ((position.x > start_x) || (std::abs(position.x - start_x) < EPSILON)) &&
            ((position.x < end_x) || (std::abs(position.x - end_x) < EPSILON)) &&
            ((position.y > start_y) || (std::abs(position.y - start_y) < EPSILON)) &&
            ((position.y < end_y) || (std::abs(position.y - end_y) < EPSILON));
};

void Roadmap::CopyContent(const Roadmap::Roads& roads) {
    for(auto& road : roads) {
        AddRoad(road);
    }
};

// ... остальные методы без изменений ...

}