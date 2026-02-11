#pragma once
 
#include <string>
#include <cstdint>
#include <memory>
#include <optional>

#include "../game_model/map.h"
#include "../game_model/dog.h"
#include "../game_model/loot_struct.h"

namespace application {

class Player {
public:
    using Id = std::uint64_t;

    Player(Id id, const model::Dog* dog, model::Map::Id map_id)
        : id_{id}
        , dog_{dog}
        , map_id_{std::move(map_id)} {
    }

    Id GetId() const;
    const pos::Position& GetPosition() const;
    const model::Dog* GetDog() const;
    std::vector<std::pair<size_t, size_t>> GetCollectedItems() const;
    int GetScore() const;
    const model::Map::Id& GetMapId() const;

private:
    Id id_;
    const model::Dog* dog_;
    model::Map::Id map_id_;
};

} // namespace application