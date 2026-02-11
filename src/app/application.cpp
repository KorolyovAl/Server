#include "application.h"
#include "../detail/random_gen.h"
 
#include <random>
#include <sstream>
#include <iomanip>

namespace {
    using namespace application;

    Application::Token GenerateToken() {    
        const std::uint64_t a = detail::GenerateRandomInt<std::uint64_t>();
        const std::uint64_t b = detail::GenerateRandomInt<std::uint64_t>();        

        std::ostringstream oss;
        oss << std::hex << std::setw(16) << std::setfill('0') << a
            << std::setw(16) << std::setfill('0') << b;

        return oss.str();
    }
}

namespace application {

    Player& Players::AddPlayer(const Player::Id id,  const model::Dog* dog, const model::Map::Id& map_id) {
        auto [it, inserted] = players_.emplace(id, Player{id, dog, map_id});
        return it->second;
    }

    const Player* Players::FindPlayerById(Player::Id id) const {
        if (auto it = players_.find(id); it != players_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const Player* Players::FindPlayerByDogName(const std::string& dog_name) const {
        for (const auto& [id, player] : players_) {
            const auto* dog = player.GetDog();
            if (dog != nullptr && dog->GetName() == dog_name) {
                return &player;
            }
        }
        return nullptr;
    }

    void PlayerTokens::SetTokenForPlayer(const Token& token, Player::Id id) {
        tokens_[token] = id;
    }

    std::optional<Player::Id> PlayerTokens::FindPlayerByToken(const Token& token) const {
        if (auto it = tokens_.find(token); it != tokens_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    Application::JoinResult Application::JoinGame(const std::string& dog_name, const std::string& map_id) {
        // 1. searching map in game_ by map_id
        const model::Map* map = game_.FindMap(model::Map::Id{map_id});
        model::GameSession& session = game_.GetSessionForMap(model::Map::Id{map_id});

        // 2. creating dog in map with name: dog_name
        model::Dog* dog = session.SpawnDog(dog_name, next_player_id_, map->GetDogsBagCapacity());

        // 3. creating player
        Player::Id id = next_player_id_++;
        Player& player = players_.AddPlayer(id, dog, model::Map::Id{map_id});

        // 4. generating token
        Token token = GenerateToken();
        tokens_.SetTokenForPlayer(token, id);

        return JoinResult{token, id};
    }

    std::optional<Player::Id> Application::FindPlayerIdByToken(const Token& token) const {
        return tokens_.FindPlayerByToken(token);
    }

    const std::deque<model::Map>& Application::GetAllMaps() const {
        return game_.GetMaps();
    }

    const model::Map* Application::FindMapByMapId(const model::Map::Id& id) const {
        return game_.FindMap(id);
    }

    const Player* Application::FindPlayerById(Player::Id id) const {
        return players_.FindPlayerById(id);
    }

    std::vector<Player> Application::GetPlayersInMap(const model::Map::Id& map_id) const {
        return players_.GetPlayersInMap(map_id);
    }

    std::vector<model::LootItem> Application::GetItemsInMap(const model::Map::Id& map_id) const {
        return game_.GetLootItemsInMap(map_id);
    }

    void Application::MovePlayer(Player::Id player_id, const pos::Direction& dir) {
        const Player* player = players_.FindPlayerById(player_id);
        if (player == nullptr) {
            return;
        }

        auto& session = game_.GetSessionForMap(player->GetMapId());

        // we need to change dog move-parameters, but Player holds const Dog*
        {
            model::Dog* dog = const_cast<model::Dog*>(player->GetDog());
            if (dog == nullptr) {
                return;
            }
            session.MoveDog(*dog, dir);
        }        
    }

    void Application::StopPlayer(Player::Id player_id) {
        const Player* player = players_.FindPlayerById(player_id);
        if (player == nullptr) {
            return;
        }

        model::Dog* dog = const_cast<model::Dog*>(player->GetDog());
        if (dog == nullptr) {
            return;
        }

        dog->SetVelocity(pos::Velocity{0.0, 0.0});
    }

    void Application::RetirePlayer(Player::Id player_id) {
        const Player* player = players_.FindPlayerById(player_id);
        if (player == nullptr) {
            return;
        }

        const model::Dog* dog = player->GetDog();
        if (dog == nullptr) {
            return;
        }

        const auto timing_it = player_timing_.find(player_id);
        const double play_time = (timing_it != player_timing_.end()) ? timing_it->second.play_time_sec : 0.0;

        PlayerRecord record;
        record.name = dog->GetName();
        record.score = dog->GetScore();
        record.play_time = play_time;
        SaveRetiredPlayerRecord(record);

        // remove from runtime state
        tokens_.RemoveTokensForPlayer(player_id);

        auto& session = game_.GetSessionForMap(player->GetMapId());
        session.RemoveDog(dog->GetId());
        players_.RemovePlayer(player_id);
        player_timing_.erase(player_id);
    }

    AppState Application::GetState() const {
        AppState app_state;

        // saving maps and sessions
        for (const auto& map : game_.GetMaps()) {
            MapState map_state;
            map_state.map_id = static_cast<std::string>(*map.GetId());

            const auto& session = game_.GetSessionForMap(map.GetId());

            // saving dogs
            for (const auto& [dog_id, dog] : session.GetAllDogs()) {
                DogState dog_state;
                dog_state.id = dog.GetId();
                dog_state.name = dog.GetName();
                dog_state.x = dog.GetCoordinates().x;
                dog_state.y = dog.GetCoordinates().y;
                dog_state.vx = dog.GetVelocity().vx;
                dog_state.vy = dog.GetVelocity().vy;

                switch (dog.GetDirection()) {
                    case pos::Direction::NORTH:
                        dog_state.dir = 'N';
                        break;
                    case pos::Direction::SOUTH:
                        dog_state.dir = 'S';
                        break;
                    case pos::Direction::EAST:
                        dog_state.dir = 'E';
                        break;
                    case pos::Direction::WEST:
                        dog_state.dir = 'W';
                        break;
                }

                dog_state.bag_capacity = dog.GetBagCapacity();
                dog_state.score = dog.GetScore();

                for (const auto& item : dog.GetCollectedItems()) {
                    DogState::BagItem bag_item;
                    bag_item.item_id = item.first;
                    bag_item.type = model::LootTypeToString(item.second.type);
                    bag_item.value = item.second.value;
                    dog_state.bag.push_back(bag_item);
                }

                map_state.dogs.push_back(dog_state);
            }

            // saving loot items
            for (const auto& loot_item : session.GetLootItems()) {
                LootState loot_state;
                loot_state.id = loot_item.id;
                loot_state.type = model::LootTypeToString(loot_item.info.type);
                loot_state.score_value = loot_item.info.value;
                loot_state.x = loot_item.coordinate.x;
                loot_state.y = loot_item.coordinate.y;
                loot_state.width = loot_item.width;

                map_state.loot.push_back(loot_state);
            }

            app_state.maps.push_back(map_state);
        }

        // saving players
        app_state.auth.next_player_id = next_player_id_;
        for (const auto& [player_id, player] : players_.GetAllPlayers()) {
            AuthState::PlayerLink player_link;
            player_link.player_id = player_id;
            player_link.map_id = static_cast<std::string>(*player.GetMapId());
            player_link.dog_id = player.GetDog()->GetId();

            if (auto it = player_timing_.find(player_id); it != player_timing_.end()) {
                player_link.play_time_sec = it->second.play_time_sec;
                player_link.idle_time_sec = it->second.idle_time_sec;
            }

            app_state.auth.players.push_back(player_link);
        }

        // saving tokens
        for (const auto& [token, player_id] : tokens_.GetAllTokens()) {
            AuthState::TokenLink token_link;
            token_link.token = token;
            token_link.player_id = player_id;

            app_state.auth.tokens.push_back(token_link);
        }

        return app_state;
    }

    void Application::RestoreState(const AppState& app_state) {
        // restoring maps and sessions
        for (const auto& map_state : app_state.maps) {
            model::Map::Id map_id{map_state.map_id};
            if (!game_.FindMap(map_id)) {
                throw std::runtime_error("Restoring state failed: map not found");
            }

            auto& session = game_.GetSessionForMap(map_id);
            session.ClearDynamicState();

            // restoring dogs
            for (const auto& dog_state : map_state.dogs) {
                model::Dog dog(dog_state.name, dog_state.id, 
                               pos::Coordinate{dog_state.x, dog_state.y},
                               dog_state.bag_capacity);

                dog.SetVelocity(pos::Velocity{dog_state.vx, dog_state.vy});

                if (dog_state.dir == 'N') {
                    dog.SetDirection(pos::Direction::NORTH);
                } 
                else if (dog_state.dir == 'S') {
                    dog.SetDirection(pos::Direction::SOUTH);
                } 
                else if (dog_state.dir == 'E') {
                    dog.SetDirection(pos::Direction::EAST);
                } 
                else if (dog_state.dir == 'W') {
                    dog.SetDirection(pos::Direction::WEST);
                } 
                else {
                    throw std::runtime_error("Restoring state failed: invalid dog direction");
                }
                
                dog.ClearItems();
                for (const auto& bag_item : dog_state.bag) {
                    model::LootType type;
                    if (bag_item.type == "key") {
                        type = model::LootType::KEY;
                    } 
                    else if (bag_item.type == "wallet") {
                        type = model::LootType::WALLET;
                    } 
                    else {
                        type = model::LootType::UNKNOWN;
                    }
                    dog.AddItem(model::LootItem{
                        .id = static_cast<model::ItemId>(bag_item.item_id),
                        .info = model::LootInfo{type, bag_item.value}
                    });
                }
                dog.AddScore(dog_state.score);
                
                session.RestoreDog(std::move(dog));
            }

            // restoring loot items
            for (const auto& loot_state : map_state.loot) {
                model::LootType type;
                if (loot_state.type == "key") {
                    type = model::LootType::KEY;
                } 
                else if (loot_state.type == "wallet") {
                    type = model::LootType::WALLET;
                } 
                else {
                    type = model::LootType::UNKNOWN;
                }
                session.RestoreLootItem(loot_state.id, 
                                        model::LootInfo{type, loot_state.score_value},
                                        pos::Coordinate{loot_state.x, loot_state.y},
                                        loot_state.width);
            }

            session.FinalizeAfterRestore();
        }

        // restoring players
        players_ = Players{};
        tokens_ = PlayerTokens{};
        next_player_id_ = app_state.auth.next_player_id;

        for (const auto& player_link : app_state.auth.players) {
            model::Map::Id map_id{player_link.map_id};
            const model::Map* map = game_.FindMap(map_id);
            if (!map) {
                throw std::runtime_error("Restoring state failed: map not found");
            }

            auto& session = game_.GetSessionForMap(map_id);
            const model::Dog* dog = session.GetDog(player_link.dog_id);
            if (!dog) {
                throw std::runtime_error("Restoring state failed: dog not found");
            }
            Player::Id player_id = player_link.player_id;
            players_.AddPlayer(player_id, dog, map_id);

            player_timing_[player_id] = PlayerTiming{player_link.play_time_sec, player_link.idle_time_sec};
        }

        for (const auto& token_link : app_state.auth.tokens) {
            tokens_.SetTokenForPlayer(token_link.token, token_link.player_id);
        }
    }

    void Application::SetOnTickCallback(OnTickCallback callback) {
        on_tick_callback_ = std::move(callback);
    }

    void Application::Tick(std::chrono::milliseconds delta) {
        // pre-tick
        const double dt = std::chrono::duration<double>(delta).count();

        std::unordered_map<Player::Id, bool> was_idle;
        was_idle.reserve(players_.GetAllPlayers().size());

        for (const auto& [player_id, player] : players_.GetAllPlayers()) {
            const auto* dog = player.GetDog();
            if (!dog) {
                continue;
            }
            const auto& v = dog->GetVelocity();
            was_idle[player_id] = (v.vx == 0.0 && v.vy == 0.0);

            auto& timing = player_timing_[player_id];
            timing.play_time_sec += dt;
        }

        // tick
        game_.Tick(delta);        

        // post-tick
        std::vector<Player::Id> to_retire;
        for (const auto& [player_id, player] : players_.GetAllPlayers()) {
            const auto* dog = player.GetDog();
            if (!dog) {
                continue;
            }

            const auto& v = dog->GetVelocity();
            const bool is_idle = (v.vx == 0.0 && v.vy == 0.0);
            auto& timing = player_timing_[player_id];

            if (is_idle) {
                const bool was_idle_before = was_idle.contains(player_id) ? was_idle.at(player_id) : false;
                if (was_idle_before) {
                    timing.idle_time_sec += dt;
                } 
                else {
                    timing.idle_time_sec = 0.0;
                }
            } 
            else {
                timing.idle_time_sec = 0.0;
            }

            if (timing.idle_time_sec >= dog_retirement_time_sec_) {
                to_retire.push_back(player_id);
            }
        }

        for (auto player_id : to_retire) {
            RetirePlayer(player_id);
        }

        if (on_tick_callback_) {
            on_tick_callback_(delta);
        }
    }

    void Application::SaveRetiredPlayerRecord(const PlayerRecord& record) {
        auto uow = uow_factory_.Create();
        uow->GetRecords().AddRecord(record);
        uow->Commit();
    }

    std::vector<PlayerRecord> Application::GetPlayerRecords(std::size_t start, std::size_t max_items) {
        auto uow = uow_factory_.Create();
        auto result = uow->GetRecords().GetRecords(start, max_items);
        uow->Commit();
        return result;
    }

} // namespace application