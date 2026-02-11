#include "game_session.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
 
// the road surface is treated as a widened corridor around the road axis
// allowed movement is computed by projecting nearby road rectangles into 1d intervals
// only roads near the current position are checked using the cell index
namespace {

static constexpr double kHalfWidth = 0.4;
static constexpr double kEps = 1e-6;

enum class RoadOrientation {
    Horizontal,
    Vertical
};

// axis-aligned rectangle in double coordinates used for road corridor checks
struct RectD {
    double min_x;
    double max_x;
    double min_y;
    double max_y;
};

// 1d interval used to represent allowed movement ranges along one axis
struct Interval {
    double left;
    double right;
};

static bool InRange(double v, double a, double b) {
    return v >= a - kEps && v <= b + kEps;
}

static RectD RoadRect(const model::Road& road) {
    if (road.IsHorizontal()) {
        const double y = static_cast<double>(road.GetStart().y);
        const double x0 = static_cast<double>(road.GetMinAlongAxis()) - kHalfWidth;
        const double x1 = static_cast<double>(road.GetMaxAlongAxis()) + kHalfWidth;

        return { x0, x1, y - kHalfWidth, y + kHalfWidth };
    }

    const double x = static_cast<double>(road.GetStart().x);
    const double y0 = static_cast<double>(road.GetMinAlongAxis()) - kHalfWidth;
    const double y1 = static_cast<double>(road.GetMaxAlongAxis()) + kHalfWidth;

    return { x - kHalfWidth, x + kHalfWidth, y0, y1 };
}

// finds the interval that contains the current coordinate
// if the coordinate is extremely close to a boundary, returns the nearest interval
static const Interval* FindCurrentInterval(
    double from,
    const std::vector<Interval>& merged)
{
    const Interval* best = nullptr;
    double best_dist = 1e100;

    for (const auto& seg : merged) {
        if (from >= seg.left - kEps && from <= seg.right + kEps) {
            return &seg;
        }

        double dist = 0.0;
        if (from < seg.left) {
            dist = seg.left - from;
        } else {
            dist = from - seg.right;
        }

        if (dist < best_dist) {
            best_dist = dist;
            best = &seg;
        }
    }

    if (best && best_dist <= kEps) {
        return best;
    }

    return nullptr;
}

// builds allowed 1d intervals for movement along the specified orientation
static std::vector<Interval> BuildIntervalsAlongRoadAxis(const model::Map& map, 
                                const pos::Coordinate& from, RoadOrientation orientation) {
    std::vector<Interval> result;
    const auto& roads = map.GetRoads();
    const auto candidates = map.GetRoadCandidates(from);

    for (size_t idx : candidates) {
        const auto& road = roads.at(idx);
        const RectD r = RoadRect(road);

        if (orientation == RoadOrientation::Horizontal) {
            if (!InRange(from.y, r.min_y, r.max_y)) {
                continue;
            }
            result.push_back({ r.min_x, r.max_x });
        }
        else {
            if (!InRange(from.x, r.min_x, r.max_x)) {
            continue;
        }
        result.push_back({ r.min_y, r.max_y });
        }        
    }

    return result;
}

// clamps movement along one axis using a set of allowed intervals
// movement is limited to the boundary of the current merged interval
double RestrictInsideIntervals(double from, double to, std::vector<Interval> intervals) {
    if (intervals.empty()) {
        return from;
    }

    std::sort(intervals.begin(), intervals.end(),
              [](const Interval& a, const Interval& b) {
                  return a.left < b.left;
              });

    std::vector<Interval> merged;
    for (const auto& seg : intervals) {
        if (merged.empty() || seg.left > merged.back().right) {
            merged.push_back(seg);
        } else {
            merged.back().right = std::max(merged.back().right, seg.right);
        }
    }

    const Interval* current = FindCurrentInterval(from, merged);

    if (!current) {
        return from;
    }

    if (to > from) {
        return std::min(to, current->right);
    }

    return std::max(to, current->left);
}

namespace collision = model::collision_detector;

class LootPickupProvider final : public collision::ItemGathererProvider {
public:
    LootPickupProvider(const model::LootStore& store)
        : store_{store} {
    }

    LootPickupProvider(const model::LootStore& store,
                       std::vector<collision::Item> items,
                       std::vector<collision::Gatherer> gatherers)
        : store_{store}
        , items_{std::move(items)}
        , gatherers_{std::move(gatherers)} {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    const model::LootStore& store_;
    std::vector<collision::Item> items_;
    std::vector<collision::Gatherer> gatherers_;
};

} // namespace

namespace model {

Map& GameSession::GetMap() {
    return map_;
}

const Map& GameSession::GetMap() const {
    return map_;
}

const Map::Id& GameSession::GetMapId() const {
    return map_.GetId();
}

std::vector<LootItem> GameSession::GetLootItems() const {
    return loot_store_.GetAllItems();
}

Dog* GameSession::SpawnDog(const std::string& dog_name, int id, int bag_capacity) {
    auto [it, inserted] = dogs_.try_emplace(id, dog_name, id, 
                                            GenerateDogSpawnCoordinates(), bag_capacity);
    if (!inserted) {
        return nullptr;
    }        
    return &it->second;
}

void GameSession::RemoveDog(int id) {
    dogs_.erase(id);
}

size_t GameSession::GetDogNumber() const {
    return dogs_.size();
}

const Dog* GameSession::GetDog(int id) const {
    auto it = dogs_.find(id);
    if (it == dogs_.end()) {
        return nullptr;
    }
    return &it->second;
}

const std::unordered_map<int, Dog>& GameSession::GetAllDogs() const {
    return dogs_;
}

void GameSession::SetRandomizeSpawnPoints(bool value) {
    randomize_spawn_points_ = value;
}

void GameSession::MoveDog(Dog& dog, const pos::Direction& dir) {
    dog.SetDirection(dir);
    const double speed = map_.GetDogSpeed();

    pos::Velocity v{0.0, 0.0};

    switch (dir) {
        case pos::Direction::NORTH:
            v = {0., -speed};
            break;
        case pos::Direction::SOUTH:
            v = {0., speed};
            break;
        case pos::Direction::WEST:
            v = {-speed, 0.};
            break;
        case pos::Direction::EAST:
            v = {speed, 0.};
            break;
    }

    dog.SetVelocity(v);
}

void GameSession::ClearDynamicState() {
    dogs_.clear();
    loot_store_.Clear();
    map_.ClearLootIndex();
}

Dog* GameSession::RestoreDog(Dog&& dog) {
    auto [it, inserted] = dogs_.try_emplace(dog.GetId(), std::move(dog));
    if (!inserted) {
        return nullptr;
    }
    return &it->second;
}

void GameSession::RestoreLootItem(ItemId id, const LootInfo info, const pos::Coordinate& coord, double width) {
    loot_store_.RestoreItem(id, info, coord, width);
    map_.AddLootItem(*loot_store_.GetItem(id));
}

void GameSession::FinalizeAfterRestore() {
    loot_store_.FinalizeAfterRestore();
}

void GameSession::SpawnLoot() {
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_loot_spawn_time_);
    size_t loot_count = loot_store_.GetItemNumber();
    size_t looter_count = dogs_.size();

    unsigned items_to_generate = loot_gen_.Generate(delta, loot_count, looter_count);

    for (unsigned i = 0; i < items_to_generate; ++i) {
        LootType type = GetRandomLootType();
        pos::Coordinate c = GenerateRandomSpawnCoordinates();

        const LootInfo info = map_.GetLootInfo(type);
        const LootItem& item = loot_store_.Create(info, c);
        map_.AddLootItem(item);
    }

    if (items_to_generate > 0) {
        last_loot_spawn_time_ = std::chrono::steady_clock::now();
    }
}

// loot event processing
void GameSession::LootEventProcessing(std::vector<collision::Gatherer> dogs) {
    std::unordered_set<Map::Cell, Map::CellHasher> checked_cells;

    // find all cells that need to be checked for loot items
    for (const auto& dog : dogs) {
        auto cells = map_.GetCellsOnTheWayArea(dog.start_pos, dog.end_pos, dog.width);
        checked_cells.insert(cells.begin(), cells.end());
    }

    // gather all items in the checked cells
    std::vector<collision::Item> items;
    for (const auto& cell : checked_cells) {
        auto ids_in_cell = map_.GetItemIdsInCell(cell);
        for (auto id : ids_in_cell) {
            const LootItem* item = loot_store_.GetItem(id);
            if (!item) {
                continue;
            }
            items.emplace_back("loot", item->coordinate, item->width, item->id);
        }
    }

    // add offices to the items list
    for (const auto& office : map_.GetOffices()) {
        pos::Coordinate office_pos {
            static_cast<double>(office.GetPosition().x),
            static_cast<double>(office.GetPosition().y)
        };
        items.emplace_back("office", office_pos, map_.GetOfficeWidth(), 0);
    }

    // find gathering events
    LootPickupProvider provider(loot_store_, items, dogs);
    auto events = collision::FindGatherEvents(provider);

    // process gathering events
    std::unordered_set<ItemId> picked;
    for (const auto& event : events) {
        if (event.item_type == "office") {
            // скидываем предметы в офис
            // и начисляем очки
            int dog_id = event.gatherer_id;
            auto dog_it = dogs_.find(dog_id);
            
            if (dog_it == dogs_.end()) {
                continue;
            }

            dog_it->second.ClearItems();
            
            continue;
        }

        // skip already collected items
        const ItemId item_id = static_cast<ItemId>(event.item_id);
        if (!picked.insert(item_id).second) {
            continue;
        }

        const auto* item = loot_store_.GetItem(event.item_id);
        if (!item) {
            continue;
        }

        // give the item to the dog
        int dog_id = event.gatherer_id;
        auto dog_it = dogs_.find(dog_id);

        if (dog_it == dogs_.end()) {
            continue;
        }

        if (dog_it->second.AddItem(*item)) {
            // remove the item from the map and loot store
            map_.RemoveLootItem(*item);
            loot_store_.Remove(event.item_id);
        }
    }
}

void GameSession::Tick(std::chrono::milliseconds delta) {
    const double dt = std::chrono::duration<double>(delta).count();
    std::vector<collision::Gatherer> dogs;

    for (auto& [id, dog] : dogs_) {
        auto pos = dog.GetPosition();
        auto& vel = pos.velocity_;
        auto& coord = pos.coordinates_;

        if (vel.vx == 0.0 && vel.vy == 0.0) {
            continue;
        }

        pos::Coordinate target {
            coord.x + vel.vx * dt,
            coord.y + vel.vy * dt
        };

        pos::Coordinate restr_pos = RestrictMovementToRoads(coord, target);

        // no movement
        if (restr_pos.x == coord.x && restr_pos.y == coord.y) {
            continue;
        }

        dog.SetCoordinates(restr_pos);

        dogs.push_back(collision::Gatherer{            
            .start_pos = coord,
            .end_pos = restr_pos,
            .width = dog.GetPickupRadius(),
            .id = dog.GetId(),
        });

        // stop on collision with road boundary
        if (restr_pos.x != target.x || restr_pos.y != target.y) {
            dog.SetVelocity(pos::Velocity{0.0, 0.0});
        }
    }

    LootEventProcessing(dogs);
    SpawnLoot();
}

pos::Coordinate GameSession::GenerateRandomSpawnCoordinates() const {
    const auto& roads = map_.GetRoads();

    if (roads.empty()) {
        return pos::Coordinate{0, 0};
    }

    const auto& road = roads[detail::GenerateRandomInt<int>(0, static_cast<int>(roads.size()) - 1)];
    if (road.IsHorizontal()) {
        const int min_x = std::min(road.GetStart().x, road.GetEnd().x);
        const int max_x = std::max(road.GetStart().x, road.GetEnd().x);
        const int x = detail::GenerateRandomInt<int>(min_x, max_x);
        return pos::Coordinate{static_cast<double>(x), static_cast<double>(road.GetStart().y)};
    }
    else {
        const int min_y = std::min(road.GetStart().y, road.GetEnd().y);
        const int max_y = std::max(road.GetStart().y, road.GetEnd().y);
        const int y = detail::GenerateRandomInt<int>(min_y, max_y);
        return pos::Coordinate{static_cast<double>(road.GetStart().x), static_cast<double>(y)};
    }
}

pos::Coordinate GameSession::GenerateDogSpawnCoordinates() const {
    if (randomize_spawn_points_) {
        return GenerateRandomSpawnCoordinates();
    }
    
    const auto& roads = map_.GetRoads();

    if (roads.empty()) {
        return pos::Coordinate{0, 0};
    }

    const auto& road = roads.at(0);
    double x = static_cast<double>(road.GetStart().x);
    double y = static_cast<double>(road.GetStart().y);
    return pos::Coordinate{x, y};    
}

LootType GameSession::GetRandomLootType() const {
    size_t types_num = map_.GetMaxCountLootTypes();

    if (types_num == 0) {
        return LootType::UNKNOWN;
    }

    size_t index = detail::GenerateRandomInt<size_t>(0, types_num - 1);
    return map_.GetLootType(index);
}

pos::Coordinate GameSession::RestrictMovementToRoads(const pos::Coordinate& from, 
                                                    const pos::Coordinate& to) const {

    const double dx = to.x - from.x;
    const double dy = to.y - from.y;

    // movement on X-axis
    if (std::abs(dx) >= std::abs(dy)) {
        auto intervals = BuildIntervalsAlongRoadAxis(map_, from, RoadOrientation::Horizontal);

        const double clamped_x = RestrictInsideIntervals(from.x, to.x, std::move(intervals));
        return pos::Coordinate{clamped_x, from.y};
    }

    // movement on Y-axis
    auto intervals = BuildIntervalsAlongRoadAxis(map_, from, RoadOrientation::Vertical);

    const double clamped_y = RestrictInsideIntervals(from.y, to.y, std::move(intervals));
    return pos::Coordinate{from.x, clamped_y};                                                             
}

} // namespace model