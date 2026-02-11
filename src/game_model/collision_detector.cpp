#include "collision_detector.h"
#include <ranges>
 
namespace model::collision_detector {

CollectionResult TryCollectPoint(pos::Coordinate a, pos::Coordinate b, pos::Coordinate c) {
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    size_t items_num = provider.ItemsCount();
    size_t gatherers_num = provider.GatherersCount();
    std::vector<GatheringEvent> events;
    events.reserve(items_num * gatherers_num);

    for (size_t g = 0; g < gatherers_num; ++g) {
        const auto& gatherer = provider.GetGatherer(g);
        for (size_t i = 0; i < items_num; ++i) {
            const auto& item_obj = provider.GetItem(i);
            CollectionResult result = TryCollectPoint(
                gatherer.start_pos,
                gatherer.end_pos,
                item_obj.position
            );
            double collect_radius = (gatherer.width + item_obj.width);

            if (result.IsCollected(collect_radius)) {
                double time = result.proj_ratio;
                events.emplace_back(item_obj.type, item_obj.id, gatherer.id, result.sq_distance, time);
            }
        }
    }

    std::ranges::sort(events, {}, &GatheringEvent::time);
    return events;
}

}  // namespace collision_detector