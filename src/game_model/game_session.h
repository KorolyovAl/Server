#pragma once

#include <list>
#include <vector>
#include <optional>
#include <chrono>
 
#include "dog.h"
#include "map.h"
#include "loot_store.h"
#include "loot_generator.h"
#include "../detail/random_gen.h"
#include "../detail/position.h"
#include "collision_detector.h"

namespace model {

// game session binds a map and a set of dogs that exist on this map
// the session is responsible for spawning, applying player movement commands
// and advancing simulation by discrete ticks
class GameSession {
public:
    explicit GameSession(Map& map, loot_gen::LootGenerator& lg) 
        : map_{map}
        , loot_gen_(lg) {
    }

    Map& GetMap();
    const Map& GetMap() const;
    const Map::Id& GetMapId() const;
    std::vector<LootItem> GetLootItems() const;
    size_t GetDogNumber() const;
    const Dog* GetDog(int id) const;
    const std::unordered_map<int, Dog>& GetAllDogs() const;

    Dog* SpawnDog(const std::string& dog_name, int id, int bag_capacity);
    void RemoveDog(int id);
    void SetRandomizeSpawnPoints(bool value);

    // updates dog direction and sets velocity according to the map dog speed
    void MoveDog(Dog& dog, const pos::Direction& dir);

    void ClearDynamicState();
    Dog* RestoreDog(Dog&& dog);
    void RestoreLootItem(ItemId id, const LootInfo info, const pos::Coordinate& coord, double width);
    void FinalizeAfterRestore();

    // advances simulation by the given time delta in model units
    void Tick(std::chrono::milliseconds delta);

private:
    pos::Coordinate GenerateRandomSpawnCoordinates() const;
    pos::Coordinate GenerateDogSpawnCoordinates() const;
    LootType GetRandomLootType() const;

    void SpawnLoot();
    void LootEventProcessing(std::vector<collision_detector::Gatherer> dogs);

    // clamps a movement segment to allowed road surface
    // returns the closest reachable point towards the target coordinate
    pos::Coordinate RestrictMovementToRoads(const pos::Coordinate& from, const pos::Coordinate& to) const;

private:
    Map& map_;
    std::unordered_map<int, Dog> dogs_;
    bool randomize_spawn_points_ = false;

    loot_gen::LootGenerator& loot_gen_;
    LootStore loot_store_;
    std::chrono::steady_clock::time_point last_loot_spawn_time_
                                    = std::chrono::steady_clock::now();
};

}