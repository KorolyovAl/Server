#pragma once

#include "../detail/position.h"

#include <algorithm>
#include <vector>
 
namespace model::collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    // квадрат расстояния до точки
    const double sq_distance;

    // доля пройденного отрезка
    const double proj_ratio;
};

// Движемся из точки a в точку b и пытаемся подобрать точку c.
// Эта функция реализована в уроке.
CollectionResult TryCollectPoint(pos::Coordinate a, pos::Coordinate b, pos::Coordinate c);

struct Item {
    std::string type;
    pos::Coordinate position;
    double width;
    int id;
};

struct Gatherer {
    pos::Coordinate start_pos;
    pos::Coordinate end_pos;
    double width;
    int id;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
};

struct GatheringEvent {
    std::string item_type;
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace model::collision_detector