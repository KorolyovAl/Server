#pragma once
 
#include <string>
#include <unordered_map>
#include <deque>
#include <optional>
#include <memory>
#include <chrono>
#include <functional>

#include "../game_model/model.h"
#include "../game_model/dog.h"
#include "../game_model/loot_struct.h"
#include "player.h"
#include "app_state.h"
#include "unit_of_work.h"

namespace application {

class Players {
public:
    Player& AddPlayer(const Player::Id id, const model::Dog* dog, const model::Map::Id& map_id);
    const Player* FindPlayerById(Player::Id id) const;
    const Player* FindPlayerByDogName(const std::string& dog_name) const;

    void RemovePlayer(Player::Id id) {
        players_.erase(id);
    }

    std::vector<Player> GetPlayersInMap(const model::Map::Id& map_id) const {
        std::vector<Player> result;
        for (const auto& [id, player] : players_) {
            if (player.GetMapId() == map_id) {
                result.emplace_back(player);
            }
        }
        return result;
    }

    const std::unordered_map<Player::Id, Player>& GetAllPlayers() const {
        return players_;
    }

private:
    std::unordered_map<Player::Id, Player> players_;
};

class PlayerTokens {
public:
    using Token = std::string;

    void SetTokenForPlayer(const Token& token, Player::Id id);
    std::optional<Player::Id> FindPlayerByToken(const Token& token) const;

    void RemoveTokensForPlayer(Player::Id id) {
        for (auto it = tokens_.begin(); it != tokens_.end();) {
            if (it->second == id) {
                it = tokens_.erase(it);
            } 
            else {
                ++it;
            }
        }
    }

    const std::unordered_map<Token, Player::Id>& GetAllTokens() const {
        return tokens_;
    }

private:
    std::unordered_map<Token, Player::Id> tokens_;    
};

class Application {
public:
    using Token = std::string;
    using OnTickCallback = std::function<void(std::chrono::milliseconds)>;

    struct JoinResult {
        Token token;
        Player::Id player_id;
    };
    
    struct PlayerTiming {
        double play_time_sec = 0.0;
        double idle_time_sec = 0.0;
    };

    explicit Application(model::Game& game, UnitOfWorkFactory& uow_factory, double dog_retirement_time)
        : game_{game}
        , uow_factory_{uow_factory} {
            dog_retirement_time_sec_ = dog_retirement_time;
    }

    JoinResult JoinGame(const std::string& dog_name, const std::string& map_id);

    std::optional<Player::Id> FindPlayerIdByToken(const Token& token) const;
    const model::Map* FindMapByMapId(const model::Map::Id& id) const;
    const Player* FindPlayerById(Player::Id id) const;

    const std::deque<model::Map>& GetAllMaps() const;
    std::vector<Player> GetPlayersInMap(const model::Map::Id& map_id) const;
    std::vector<model::LootItem> GetItemsInMap(const model::Map::Id& map_id) const;
    AppState GetState() const;

    void RestoreState(const AppState& app_state);

    void SetOnTickCallback(OnTickCallback callback);

    void Tick(std::chrono::milliseconds delta);

    void MovePlayer(Player::Id player_id, const pos::Direction& dir);
    void StopPlayer(Player::Id player_id);

    void SaveRetiredPlayerRecord(const PlayerRecord& record);
    std::vector<PlayerRecord> GetPlayerRecords(std::size_t start, std::size_t max_items);

private:
    void RetirePlayer(Player::Id player_id);

private:
    model::Game& game_;
    UnitOfWorkFactory& uow_factory_;
    Players players_;
    PlayerTokens tokens_;
    Player::Id next_player_id_ = 0;
    double dog_retirement_time_sec_ = 60.0;
    std::unordered_map<Player::Id, PlayerTiming> player_timing_;

    OnTickCallback on_tick_callback_;
};

} // namespace application