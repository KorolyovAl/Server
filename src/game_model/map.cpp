#include "map.h"
#include <algorithm>
#include <cmath>
#include <cassert>

namespace model {

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}
 
bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

// min coordinate along the road direction
Coord Road::GetMinAlongAxis() const noexcept {
    return IsHorizontal()
            ? std::min(start_.x, end_.x)
            : std::min(start_.y, end_.y);
}

// max coordinate along the road direction
Coord Road::GetMaxAlongAxis() const noexcept {
    return IsHorizontal()
            ? std::max(start_.x, end_.x)
            : std::max(start_.y, end_.y);
}

void Map::AddRoad(const Road& road) {
    const size_t index = roads_.size();
    roads_.emplace_back(road);
    IndexingRoadInCells(index);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::RebuildRoadCellIndex() {
    roads_by_cell_.clear();
    roads_by_cell_.reserve(roads_.size() * 2);

    for (size_t i = 0; i < roads_.size(); ++i) {
        IndexingRoadInCells(i);
    }
}

void Map::IndexingRoadInCells(size_t road_index) {
    const Road& r = roads_.at(road_index);
    const auto& a = r.GetStart();
    const auto& b = r.GetEnd();

    if (r.IsHorizontal()) {
        const int y = a.y;
        const int x0 = std::min(a.x, b.x);
        const int x1 = std::max(a.x, b.x);

        // index all cells along the horizontal axis line
        for (int x = x0; x <= x1; ++x) {
            roads_by_cell_[Cell{x, y}].push_back(road_index);
        }

        // touch adjacent rows to reduce misses near the road width boundary
        roads_by_cell_[Cell{x0, y - 1}];
        roads_by_cell_[Cell{x0, y + 1}];
        return;
    }

    const int x = a.x;
    const int y0 = std::min(a.y, b.y);
    const int y1 = std::max(a.y, b.y);

    // index all cells along the vertical axis line
    for (int y = y0; y <= y1; ++y) {
        roads_by_cell_[Cell{x, y}].push_back(road_index);
    }
}

// gathers candidate roads using the cell index
// neighbor cells are used to avoid missing roads when the point is close to cell borders
std::vector<size_t> Map::GetRoadCandidates(const pos::Coordinate& c) const {
    const int cx = static_cast<int>(std::floor(c.x));
    const int cy = static_cast<int>(std::floor(c.y));

    size_t reserve_hint = 8;
    std::vector<size_t> result;
    result.reserve(reserve_hint);

    auto add_from_cell = [&](int x, int y) {
        auto it = roads_by_cell_.find(Cell{x, y});
        if (it == roads_by_cell_.end()) {
            return;
        }
        const auto& vec = it->second;
        result.insert(result.end(), vec.begin(), vec.end());
    };

    // base cell and immediate neighbors
    // this covers most cases where the road rectangle intersects the query line
    add_from_cell(cx, cy);
    add_from_cell(cx + 1, cy);
    add_from_cell(cx, cy + 1);
    add_from_cell(cx + 1, cy + 1);

    // additional cases
    add_from_cell(cx - 1, cy);
    add_from_cell(cx, cy - 1);

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}

void Map::AddLootType(const std::string& type, int value) {
    if (type == "key") {
        loot_info_.push_back({LootType::KEY, value});
    }
    else if (type == "wallet") {
        loot_info_.push_back({LootType::WALLET, value});
    }
    else {
        assert(false);
    }
}

void Map::AddLootItem(const LootItem& item) {
    const int cx = static_cast<int>(std::floor(item.coordinate.x));
    const int cy = static_cast<int>(std::floor(item.coordinate.y));
    items_by_cell_[Cell{cx, cy}].insert(item.id);
}

void Map::RemoveLootItem(const LootItem& item) {
    const int cx = static_cast<int>(std::floor(item.coordinate.x));
    const int cy = static_cast<int>(std::floor(item.coordinate.y));
    auto it = items_by_cell_.find(Cell{cx, cy});
    if (it != items_by_cell_.end()) {
        it->second.erase(item.id);
    }
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

const double Map::GetOfficeWidth() const noexcept {
    return base_width_;
}

double Map::GetDogSpeed() const {
    return dog_speed_;
}

int Map::GetDogsBagCapacity() const {
    return dogs_bag_capacity_;
}

size_t Map::GetMaxCountLootTypes() const {
    return loot_info_.size();
}

LootType Map::GetLootType(size_t index) const {
    return loot_info_.at(index).type;
}

LootInfo Map::GetLootInfo(LootType const type) const {
    for (const auto& info : loot_info_) {
        if (info.type == type) {
            return info;
        }
    }
    return {};
}

const std::vector<LootType> Map::GetAllLootTypes() const {
    std::vector<LootType> types;
    for (const auto& t : loot_info_) {
        types.push_back(t.type);
    }
    return types;
}

std::vector<ItemId> Map::GetItemIdsInCell(const Cell& cell) const {
    std::vector<ItemId> result;
    auto it = items_by_cell_.find(cell);
    if (it != items_by_cell_.end()) {
        result.insert(result.end(), it->second.begin(), it->second.end());
    }
    return result;
}

std::vector<Map::Cell> Map::GetCellsOnTheWayArea(const pos::Coordinate& from, const pos::Coordinate& to, 
                                                 double width_area) const {
    const int min_x = static_cast<int>(std::floor(
        std::min(from.x, to.x) - width_area
    ));
    const int max_x = static_cast<int>(std::floor(
        std::max(from.x, to.x) + width_area
    ));
    const int min_y = static_cast<int>(std::floor(
        std::min(from.y, to.y) - width_area
    ));
    const int max_y = static_cast<int>(std::floor(
        std::max(from.y, to.y) + width_area
    ));

    std::vector<Cell> result;
    result.reserve((max_x - min_x + 1) * (max_y - min_y + 1));

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            result.push_back(Cell{x, y});
        }
    }

    return result;
}

} // namespace model