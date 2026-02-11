#pragma once

#include "../detail/position.h"

#include <stdexcept>
#include <string>

namespace model {
 
using ItemId = uint32_t;

enum class LootType {
    KEY,
    WALLET,
    UNKNOWN
};

struct LootInfo {
    LootType type = LootType::UNKNOWN;
    int value = 0;
};

struct LootItem {
    ItemId id = 0;
    LootInfo info;
    pos::Coordinate coordinate;
    double width = 0.0;
};

inline std::string LootTypeToString(const LootType type) {
    switch (type) {
        case LootType::KEY:
            return "key";
        case LootType::WALLET:
            return "wallet";
        default:
            throw std::invalid_argument("incorrect loot type");
    }
}

} // namespace model