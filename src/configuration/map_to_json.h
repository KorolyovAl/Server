#pragma once

#include <boost/json.hpp>

#include "../game_model/model.h"
#include "../metadata/loot_data.h"

namespace config {

namespace json = boost::json;

inline json::object MakeRoadAsJson(const model::Road& road) {
    json::object road_obj;
    model::Point start = road.GetStart();
    model::Point end = road.GetEnd();
    road_obj["x0"] = start.x;
    road_obj["y0"] = start.y;

    if (road.IsHorizontal()) {
        road_obj["x1"] = end.x;
    }
    else {
        road_obj["y1"] = end.y;
    }
    
    return road_obj;
}

inline json::object MakeBuildingAsJson(const model::Building& building) {
    json::object building_obj;
    const model::Rectangle& bounds = building.GetBounds();
    building_obj["x"] = bounds.position.x;
    building_obj["y"] = bounds.position.y;
    building_obj["w"] = bounds.size.width;
    building_obj["h"] = bounds.size.height;

    return building_obj;
}
 
inline json::object MakeOfficeAsJson(const model::Office& office) {
    json::object office_obj;
    office_obj["id"] = static_cast<std::string>(*office.GetId());
    model::Point position = office.GetPosition();
    office_obj["x"] = position.x;
    office_obj["y"] = position.y;
    model::Offset offset = office.GetOffset();
    office_obj["offsetX"] = offset.dx;
    office_obj["offsetY"] = offset.dy;

    return office_obj;
}

inline json::object GetMapAsJsonObject(const model::Map* map_ptr, const metadata::LootMetaPerMap& loot_metadata) {
    json::object map;
    map["id"] = static_cast<std::string>(*map_ptr->GetId());
    map["name"] = map_ptr->GetName();
    
    json::array roads_array;
    for (const auto& road : map_ptr->GetRoads()) {            
        roads_array.push_back(MakeRoadAsJson(road));
    }
    map["roads"] = roads_array;

    json::array buildings_array;
    for (const auto& building : map_ptr->GetBuildings()) {
        buildings_array.push_back(MakeBuildingAsJson(building));
    }
    map["buildings"] = buildings_array;

    json::array offices_array;
    for (const auto& office : map_ptr->GetOffices()) {
        offices_array.push_back(MakeOfficeAsJson(office));
    }
    map["offices"] = offices_array;

    json::array items_array;
    for (const auto& item : loot_metadata.items.at(static_cast<std::string>(*map_ptr->GetId()))) {
        items_array.push_back(item);
    }
    map["lootTypes"] = items_array;

    return map;
}

} // namespace config