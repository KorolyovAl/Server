#pragma once

#include <filesystem>
#include <memory>

#include "../game_model/model.h"
#include "../metadata/loot_data.h"
 
namespace json_loader {

struct GameSettings {
    std::unique_ptr<model::Game> game;
    double dog_retirement_time_sec = 60.0;
};

GameSettings LoadGame(const std::filesystem::path& json_path, metadata::LootMetaPerMap& loot_meta);

}  // namespace json_loader
