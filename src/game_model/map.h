#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <cstdint>

#include "loot_struct.h"
#include "../detail/tagged.h"
#include "../detail/position.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept;
    bool IsVertical() const noexcept;

    Point GetStart() const noexcept;
    Point GetEnd() const noexcept;

    // min/max coordinates along the road axis
    Coord GetMinAlongAxis() const noexcept;
    Coord GetMaxAlongAxis() const noexcept;

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    // integer grid cell used for road spatial index
    struct Cell {
        int x = 0;
        int y = 0;

        bool operator==(const Cell& other) const noexcept {
            return x == other.x && y == other.y;
        }
    };

    // hash for Cell to use in unordered_map
    struct CellHasher {
        size_t operator()(const Cell& c) const noexcept {
            const uint64_t ux = static_cast<uint32_t>(c.x);
            const uint64_t uy = static_cast<uint32_t>(c.y);
            return static_cast<size_t>((ux << 32) ^ uy);
        }
    };

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;
    const double GetOfficeWidth() const noexcept;
    double GetDogSpeed() const;
    int GetDogsBagCapacity() const;
    size_t GetMaxCountLootTypes() const;
    LootType GetLootType(size_t index) const;
    LootInfo GetLootInfo(LootType const type) const;
    const std::vector<LootType> GetAllLootTypes() const;
    std::vector<ItemId> GetItemIdsInCell(const Cell& cell) const;
    std::vector<Cell> GetCellsOnTheWayArea(const pos::Coordinate& from, const pos::Coordinate& to, 
                                            double width_area) const;

    // returns road indices that may contain the given coordinate
    // the result is used to avoid linear scan over all roads
    std::vector<size_t> GetRoadCandidates(const pos::Coordinate& c) const;

    // rebuilds the cell index after bulk road loading
    void RebuildRoadCellIndex();

    // adds a road and updates the cell index
    void AddRoad(const Road& road);
    void AddBuilding(const Building& building);
    void AddOffice(Office office);
    void AddLootType(const std::string& type, int value);
    void AddLootItem(const LootItem& item);

    void RemoveLootItem(const LootItem& item);

    void ClearLootIndex() {
        items_by_cell_.clear();
    }

    void SetDogSpeed(double speed) {
        dog_speed_ = speed;
    }

private:
    // puts the road into all grid cells covered by its axis line
    void IndexingRoadInCells(size_t road_index);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    std::vector<LootInfo> loot_info_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    // spatial index for fast lookup of nearby roads
    std::unordered_map<Cell, std::vector<size_t>, CellHasher> roads_by_cell_;
    // spatial index for fast lookup of nearby items (contains item IDs)
    std::unordered_map<Cell, std::set<size_t>, CellHasher> items_by_cell_;

    // constants
    double dog_speed_ = 1.0;
    double base_width_ = 0.5;
    int dogs_bag_capacity_ = 3;    
};

}  // namespace model 