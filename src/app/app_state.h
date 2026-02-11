#pragma once
 
#include <cstdint>
#include <string>
#include <vector>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

namespace application {

struct DogState {
    int id;
    std::string name;

    double x, y;
    double vx, vy;
    char dir; // N, S, E, W

    int bag_capacity;
    int score;

    struct BagItem {
        std::uint64_t item_id;
        std::string type;
        int value;

        template<class Archive>
        void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
            ar & item_id;
            ar & type;
            ar & value;
        }
    };

    std::vector<BagItem> bag;

    template<class Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
        ar & id;
        ar & name;
        ar & x;
        ar & y;
        ar & vx;
        ar & vy;
        ar & dir;
        ar & bag_capacity;
        ar & score;
        ar & bag;
    }
};

struct LootState {
    std::uint64_t id;
    std::string type;
    int score_value;
    double x, y;
    double width;

    template<class Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
        ar & id;
        ar & type;
        ar & score_value;
        ar & x;
        ar & y;
        ar & width;
    }
};

struct MapState {
    std::string map_id;
    std::vector<DogState> dogs;
    std::vector<LootState> loot;

    template<class Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
        ar & map_id;
        ar & dogs;
        ar & loot;
    }
};

struct AuthState {
    struct PlayerLink {
        std::uint64_t player_id;
        std::string map_id;
        int dog_id;

        double play_time_sec = 0.0;
        double idle_time_sec = 0.0;

        template<class Archive>
        void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
            ar & player_id;
            ar & map_id;
            ar & dog_id;
            ar & play_time_sec;
            ar & idle_time_sec;
        }
    };

    struct TokenLink {
        std::string token;
        std::uint64_t player_id;

        template<class Archive>
        void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
            ar & token;
            ar & player_id;
        }
    };

    std::uint64_t next_player_id;
    std::vector<PlayerLink> players;
    std::vector<TokenLink> tokens;

    template<class Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
        ar & next_player_id;
        ar & players;
        ar & tokens;
    }
};

struct AppState {
    AuthState auth;
    std::vector<MapState> maps;

    template<class Archive>
    void serialize(Archive &ar, [[maybe_unused]] const unsigned int version) {
        ar & auth;
        ar & maps;
    }
};
    
} // namespace application