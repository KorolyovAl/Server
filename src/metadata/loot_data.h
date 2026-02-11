#pragma once

#include <string>
#include <map>

#include <boost/json.hpp>

namespace metadata {

// collect all data for frontend as json, required for statistics request
using LootType = std::string;
using MapId = std::string;

using LootItemMetadata = boost::json::object;

struct LootMetaPerMap {
    std::map<MapId, std::vector<LootItemMetadata>> items;
};

} // namespace metadata 