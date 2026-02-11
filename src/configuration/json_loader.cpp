#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <string>
#include <vector>
#include <optional>
#include <map>

//#include <iostream>

namespace {
    namespace json = boost::json;

    struct RoadConfig {
        int x0 = 0;
        int y0 = 0;
        int end = 0;
        bool is_horizontal = false;
    };
    
    struct BuildingConfig {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    struct OfficeConfig {
        std::string id;
        int x = 0;
        int y = 0;
        int offsetX = 0;
        int offsetY = 0;
    };

    struct LootItemConfig {
        json::object item;
    };

    struct MapConfig {
        std::string id;
        std::string name;
        std::vector<RoadConfig> roads;
        std::vector<BuildingConfig> buildings;
        std::vector<OfficeConfig> offices;
        std::optional<double> dog_speed;
        std::optional<int> bag_capacity;
        std::vector<LootItemConfig> items;
    };

    struct LootGeneratorConfig {
        int period = 0;
        double probability = 0.0;
    };

    struct GameConfig {
        std::vector<MapConfig> maps;
        std::optional<double> default_dog_speed;
        std::optional<double> default_retirement_time;
        std::optional<int> default_bag_capacity;        
        LootGeneratorConfig loot_gen;
    };

    RoadConfig tag_invoke(json::value_to_tag<RoadConfig>, const json::value& value) {
        const json::object& obj = value.as_object();
        RoadConfig road;
        road.x0 = obj.at("x0").as_int64();
        road.y0 = obj.at("y0").as_int64();

        if (const json::value* v = obj.if_contains("x1")) {
            road.is_horizontal = true;
            road.end = v->as_int64();
        }
        else if (const json::value* v = obj.if_contains("y1")) {
            road.is_horizontal = false;
            road.end = v->as_int64();
        }
        else {
            throw std::runtime_error("Invalid road object: missing 'x1' or 'y1' field");
        }

        return road;
    }

    BuildingConfig tag_invoke(json::value_to_tag<BuildingConfig>, const json::value& value) {
        const json::object& obj = value.as_object();

        BuildingConfig building;
        building.x = obj.at("x").as_int64();
        building.y = obj.at("y").as_int64();
        building.w = obj.at("w").as_int64();
        building.h = obj.at("h").as_int64();

        return building;
    }

    OfficeConfig tag_invoke(json::value_to_tag<OfficeConfig>, const json::value& value) {
        const json::object& obj = value.as_object();

        OfficeConfig office;
        office.id = obj.at("id").as_string().c_str();
        office.x = obj.at("x").as_int64();
        office.y = obj.at("y").as_int64();
        office.offsetX = obj.at("offsetX").as_int64();
        office.offsetY = obj.at("offsetY").as_int64();

        return office;
    }

    LootItemConfig tag_invoke(json::value_to_tag<LootItemConfig>, const json::value& value) {
        LootItemConfig config;
        config.item = value.as_object();

        return config;
    }

    MapConfig tag_invoke(json::value_to_tag<MapConfig>, const json::value& value) {
        const json::object& obj = value.as_object();
        
        MapConfig map;
        map.id = obj.at("id").as_string().c_str();
        map.name = obj.at("name").as_string().c_str();

        // Add roads
        const json::array& roads_array = obj.at("roads").as_array();
        for (const auto& road_value : roads_array) {
            RoadConfig road = json::value_to<RoadConfig>(road_value);
            map.roads.push_back(std::move(road));
        }

        // Add buildings
        const json::array& buildings_array = obj.at("buildings").as_array();
        for (const auto& building_value : buildings_array) {
            BuildingConfig building = json::value_to<BuildingConfig>(building_value);
            map.buildings.push_back(std::move(building));
        }

        // Add offices
        const json::array& offices_array = obj.at("offices").as_array();
        for (const auto& office_value : offices_array) {
            OfficeConfig office = json::value_to<OfficeConfig>(office_value);
            map.offices.push_back(std::move(office));
        }

        // Add items data
        const json::array& items_array = obj.at("lootTypes").as_array();
        for (const auto& item_value : items_array) {
            LootItemConfig item = json::value_to<LootItemConfig>(item_value);
            map.items.push_back(item);
        }

        // Dog speed (optional)
        if (auto it = obj.find("dogSpeed"); it != obj.end()) {
            map.dog_speed = json::value_to<double>(it->value());
        }

        // Bag capacity (optional)
        if (auto it = obj.find("bagCapacity"); it != obj.end()) {
            map.bag_capacity = json::value_to<int>(it->value());
        }

        return map;
    }

    LootGeneratorConfig tag_invoke(json::value_to_tag<LootGeneratorConfig>, const json::value& value) {
        const json::object& obj = value.as_object();
        LootGeneratorConfig config;
        config.period = json::value_to<int>(obj.at("period"));
        config.probability = json::value_to<double>(obj.at("probability"));

        return config;
    }

    GameConfig tag_invoke(json::value_to_tag<GameConfig>, const json::value& value) {
        const json::object& obj = value.as_object();
        GameConfig game;
        game.maps = json::value_to<std::vector<MapConfig>>(obj.at("maps"));

        if (auto it = obj.find("defaultDogSpeed"); it != obj.end()) {
            game.default_dog_speed = json::value_to<double>(it->value());
        }

        if (auto it = obj.find("defaultBagCapacity"); it != obj.end()) {
            game.default_bag_capacity = json::value_to<int>(it->value());
        }

        if (auto it = obj.find("dogRetirementTime"); it != obj.end()) {
            game.default_retirement_time = json::value_to<double>(it->value());
        }

        game.loot_gen = json::value_to<LootGeneratorConfig>(obj.at("lootGeneratorConfig"));

        return game;
    }

    void NormalizeGameParameters(GameConfig& cfg) {
        const double global_speed = cfg.default_dog_speed.value_or(1.0);
        const int global_capacity = cfg.default_bag_capacity.value_or(3);

        for (auto& map : cfg.maps) {
            if (!map.dog_speed) {
                map.dog_speed = global_speed;
            }

            if (!map.bag_capacity) {
                map.bag_capacity = global_capacity;
            }
        }
    }    

    model::Map ConstructMapFromConfig(const MapConfig& map_cfg) {
        model::Map map{model::Map::Id{map_cfg.id}, map_cfg.name};

        // roads
        for (const auto& r : map_cfg.roads) {
            model::Point start{
                static_cast<model::Coord>(r.x0),
                static_cast<model::Coord>(r.y0)
            };

            if (r.is_horizontal) {
                map.AddRoad(model::Road{
                    model::Road::HORIZONTAL,
                    start,
                    static_cast<model::Coord>(r.end)
                });
            } 
            else {
                map.AddRoad(model::Road{
                    model::Road::VERTICAL,
                    start,
                    static_cast<model::Coord>(r.end)
                });
            }
        }

        // buildings
        for (const auto& b : map_cfg.buildings) {
            model::Rectangle rect{
                {static_cast<model::Coord>(b.x), static_cast<model::Coord>(b.y)},
                {static_cast<model::Dimension>(b.w), static_cast<model::Dimension>(b.h)}
            };
            map.AddBuilding(model::Building{rect});
        }

        // offices
        for (const auto& o : map_cfg.offices) {
            map.AddOffice(model::Office{
                model::Office::Id{o.id},
                {
                    static_cast<model::Coord>(o.x),
                    static_cast<model::Coord>(o.y)
                },
                {
                    static_cast<model::Dimension>(o.offsetX),
                    static_cast<model::Dimension>(o.offsetY)
                }
            });
        }

        // items
        for (const auto& item_json : map_cfg.items) {
            map.AddLootType(item_json.item.at("name").as_string().c_str(), 
                            item_json.item.at("value").as_int64());
        }

        map.SetDogSpeed(map_cfg.dog_speed.value_or(1.0));
        map.RebuildRoadCellIndex();

        return map;
    }
} // namespace

namespace json_loader {

    namespace json = boost::json;

    json::value ReadJsonFromFile(const std::filesystem::path& json_path) {
        std::ifstream input(json_path);
        if (!input.is_open()) {
            throw std::runtime_error("Could not open JSON file: " + json_path.string());
        }

        std::string json_content((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());

        boost::json::error_code ec;
        json::value jv = json::parse(json_content, ec);
        if (ec) {
            throw std::runtime_error("Failed to parse JSON file: " + ec.message());
        }

        return jv;
    } 

    GameSettings LoadGame(const std::filesystem::path& json_path, metadata::LootMetaPerMap& loot_meta) {  
        json::value value = ReadJsonFromFile(json_path);
        GameConfig config = json::value_to<GameConfig>(value);
        NormalizeGameParameters(config);

        loot_gen::LootGenerator loot_gen(
                                    static_cast<std::chrono::milliseconds>(config.loot_gen.period),
                                    config.loot_gen.probability
        );
        auto game = std::make_unique<model::Game>(std::move(loot_gen));

        for (const auto& map_cfg : config.maps) {        
            game->AddMap(ConstructMapFromConfig(map_cfg));

            // save all items metadata as json
            for (const auto& item_json : map_cfg.items) {
                loot_meta.items[map_cfg.id].push_back(item_json.item);
            }
        }
        game->BuildSessions();

        const double raw_retire = config.default_retirement_time.value_or(60.0);
        const double retire_time = (raw_retire > 0.0) ? raw_retire : 60.0;

        return GameSettings{std::move(game), retire_time};;
    }

}  // namespace json_loader
