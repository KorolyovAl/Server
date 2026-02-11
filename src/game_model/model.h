#pragma once
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <chrono>

#include "game_session.h"
#include "map.h"
#include "loot_generator.h"

namespace model {

class Game {
public:
    using Maps = std::deque<Map>;

    Game(loot_gen::LootGenerator&& loot_gen) 
            : loot_gen_(std::move(loot_gen)) {
    }
 
    void AddMap(Map map);    

    const Maps& GetMaps() const noexcept;
    GameSession& GetSessionForMap(const Map::Id& id);
    const GameSession& GetSessionForMap(const Map::Id& id) const;
    std::vector<LootItem> GetLootItemsInMap(const Map::Id& id) const;

    void SetRandomizeSpawnPoints(bool value);

    const Map* FindMap(const Map::Id& id) const noexcept;
    void BuildSessions();
    void Tick(std::chrono::milliseconds delta);

private:
    void CreateSessionForMap(const Map::Id& id) {
        sessions_.emplace_back(maps_.at(map_id_to_index_.at(id)), loot_gen_);
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    MapIdToIndex map_id_to_index_;
    
    std::vector<GameSession> sessions_;
    loot_gen::LootGenerator loot_gen_;
    bool randomize_spawn_points_ = false;
};

}  // namespace model
