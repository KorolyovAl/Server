#include "api_handler.h"

#include <string>
#include <charconv>
#include <cctype>
#include <cassert>
#include <algorithm>
#include <boost/json.hpp>

#include "make_response.h"
#include "path_handler.h"
#include "../detail/position.h"
#include "../configuration/map_to_json.h"

namespace {

namespace json = boost::json;
using namespace http_handler;

bool IsValidToken(std::string_view token) {
    if (token.size() != 32) {
        return false;
    }

    return std::ranges::all_of(token, [](char ch) {
        return std::isxdigit(static_cast<unsigned char>(ch));
    });
}

std::string DirectionToLetter(pos::Direction dir) {
    switch (dir) {
        case pos::Direction::NORTH:
            return "U";
        case pos::Direction::SOUTH:
            return "D";
        case pos::Direction::WEST:
            return "L";
        case pos::Direction::EAST:
            return "R";
    }

    throw std::invalid_argument("Invalid direction");
}

pos::Direction LetterToDirection(const std::string& letter) {
    if (letter == "U") {
        return pos::Direction::NORTH;
    }
    else if (letter == "D") {
        return pos::Direction::SOUTH;
    }
    else if (letter == "L") {
        return pos::Direction::WEST;
    }
    else if (letter == "R") {
        return pos::Direction::EAST;
    }
    throw std::invalid_argument("Invalid direction letter");

}

std::optional<std::size_t> ParseSize(std::string_view s) {
    std::size_t value = 0;
    auto first = s.data();
    auto last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

} // namespace

namespace http_handler {

namespace json = boost::json;
namespace fs = std::filesystem;

json::array ApiHandler::GetAllMapsAsJsonArray() const {
    json::array maps_array;

    for (auto& map_obj : app_.GetAllMaps()) {
        json::object map;
        map["id"] = static_cast<std::string>(*map_obj.GetId());
        map["name"] = map_obj.GetName();
        maps_array.push_back(std::move(map));
    }

    return maps_array;
}

std::optional<StringResponse> ApiHandler::CheckAuthorizationAndToken(const StringRequest& request, application::Player::Id& player_id) const {
    // check authorization header
    auto it = request.find(http::field::authorization);
    if (it == request.end()) {
        return MakeErrorResponse(http::status::unauthorized,
            "invalidToken", "Authorization header is missing", request);
    }

    // check bearer
    std::string_view auth = it->value();
    constexpr std::string_view prefix = "Bearer ";

    if (!auth.starts_with(prefix) || auth.size() <= prefix.size()) {
        return MakeErrorResponse(http::status::unauthorized,
            "invalidToken", "Authorization header is invalid", request);
    }
    
    // check token
    std::string token = std::string(auth.substr(prefix.size()));
    if (!IsValidToken(token)) {
        return MakeErrorResponse(http::status::unauthorized,
            "invalidToken", "Authorization header is invalid", request);
    }

    auto player_id_opt = app_.FindPlayerIdByToken(token);
    if (!player_id_opt) {
        return MakeErrorResponse(http::status::unauthorized,
            "unknownToken", "Player token has not been found", request);
    }

    // find player
    player_id = *player_id_opt;

    return std::nullopt;
}

StringResponse ApiHandler::HandleJoinGame(const StringRequest& request) {
    // check method
    if (request.method() != http::verb::post) {
        StringResponse res = MakeErrorResponse(http::status::method_not_allowed, 
                                                "invalidMethod", "Only POST method is expected", request);
        res.set(http::field::allow, "POST");
        return res;
    }

    // check content-type
    auto content_type_it = request.find(http::field::content_type);
    if (content_type_it == request.end() || content_type_it->value() != ContentType::JSON) {
        return MakeErrorResponse(http::status::bad_request, 
                                "invalidContentType", "Content-Type must be application/json", request);
    }

    // parse body
    json::value body_json;
    std::string user_name;
    std::string map_id;

    try {
        body_json = json::parse(request.body());

        if (!body_json.is_object()) {
            throw std::invalid_argument("");
        }

        const auto& obj = body_json.as_object();

        // get user_name and map_id
        const auto* user_name_it = obj.if_contains("userName");
        const auto* map_id_it = obj.if_contains("mapId");

        if (!user_name_it || !map_id_it || !user_name_it->is_string() || !map_id_it->is_string()) {
            throw std::invalid_argument("");
        }

        user_name = std::string(user_name_it->as_string().c_str());
        map_id = std::string(map_id_it->as_string().c_str());
    }
    catch (const std::exception&) {
        return MakeInvalidArgument(request, "Join game request parse error");
    }

    //  check if user_name is empty
    if (user_name.empty()) {
        return MakeInvalidArgument(request, "Invalid name");
    }

    // check if map exists
    const model::Map* map_ptr = app_.FindMapByMapId(model::Map::Id(map_id));
    if (!map_ptr) {
        return MakeErrorResponse(http::status::not_found,
            "mapNotFound", "Map not found", request);
    }

    // join game
    application::Application::JoinResult join_result = app_.JoinGame(user_name, map_id);

    // join response
    json::object resp_obj;
    resp_obj["authToken"] = join_result.token;
    resp_obj["playerId"]  = join_result.player_id;
    std::string body = json::serialize(resp_obj);

    return MakeStringResponse(http::status::ok, body, request.version(),
                                            request.keep_alive(), ContentType::JSON);
}

StringResponse ApiHandler::HandleGetPlayers(const StringRequest& request) const {
    // check method
    if (request.method() != http::verb::get && request.method() != http::verb::head) {
        StringResponse res = MakeErrorResponse(http::status::method_not_allowed,
            "invalidMethod", "Only GET and HEAD methods are expected", request);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    application::Player::Id player_id;
    auto check_token_res = CheckAuthorizationAndToken(request, player_id);
    if (check_token_res) {
        return *check_token_res;
    }

    const application::Player* player = app_.FindPlayerById(player_id);

    json::object result;
    for (const auto& p : app_.GetPlayersInMap(player->GetMapId())) {
        const model::Dog* dog = p.GetDog();
        if (dog) {
            json::object dog_obj;
            dog_obj["name"] = dog->GetName();
            result[std::to_string(p.GetId())] = dog_obj;
        }
    }

    std::string body = json::serialize(result);
    StringResponse res = MakeStringResponse(http::status::ok, body, request.version(),
                                            request.keep_alive(), ContentType::JSON);
    res.set(http::field::cache_control, "no-cache");
    
    if (request.method() == http::verb::head) {
        res.body().clear();
    }

    return res;
}

StringResponse ApiHandler::HandleGetGameState(const StringRequest& request) const {
    // check method
    if (request.method() != http::verb::get && request.method() != http::verb::head) {
        StringResponse res = MakeErrorResponse(http::status::method_not_allowed,
            "invalidMethod", "Only GET and HEAD methods are expected", request);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    application::Player::Id player_id;
    auto check_token_res = CheckAuthorizationAndToken(request, player_id);
    if (check_token_res) {
        return *check_token_res;
    }

    const application::Player* player = app_.FindPlayerById(player_id);

    json::object root;
    json::object players_obj;
    for (const auto& p : app_.GetPlayersInMap(player->GetMapId())) {
        json::object player_pos;
        const pos::Position& pos = p.GetPosition();

        json::array coordinates_arr;
        coordinates_arr.push_back(pos.coordinates_.x);
        coordinates_arr.push_back(pos.coordinates_.y);

        json::array velocity_arr;
        velocity_arr.push_back(pos.velocity_.vx);
        velocity_arr.push_back(pos.velocity_.vy);

        player_pos["pos"] = coordinates_arr;
        player_pos["speed"] = velocity_arr;
        player_pos["dir"] = DirectionToLetter(pos.direction_);

        std::vector<std::pair<size_t, size_t>> collected_items = p.GetCollectedItems();
        json::array collected_items_arr;
        for (const auto& item : collected_items) {
            json::object item_obj;
            item_obj["id"] = item.first; // id
            item_obj["type"] = item.second; // type
            collected_items_arr.push_back(item_obj);
        }
        player_pos["bag"] = collected_items_arr;
        player_pos["score"] = p.GetScore();

        players_obj[std::to_string(p.GetId())] = std::move(player_pos);
    }
    root["players"] = std::move(players_obj);

    json::object items_obj;
    size_t item_counter = 0;
    for (const auto& item : app_.GetItemsInMap(player->GetMapId())) {
        json::object item_json;
        
        json::array coordinates_arr;
        coordinates_arr.push_back(item.coordinate.x);
        coordinates_arr.push_back(item.coordinate.y);

        item_json["type"] = static_cast<int>(item.info.type);
        item_json["pos"] = coordinates_arr;

        items_obj[std::to_string(item_counter)] = item_json;
        ++item_counter;
    }
    root["lostObjects"] = std::move(items_obj);

    std::string body = json::serialize(root);
    StringResponse res = MakeStringResponse(http::status::ok, body, request.version(),
                                            request.keep_alive(), ContentType::JSON);
    res.set(http::field::cache_control, "no-cache");
    
    if (request.method() == http::verb::head) {
        res.body().clear();
    }

    return res;
}

StringResponse ApiHandler::HandleGetRecords(const StringRequest& request) const {
    if (request.method() != http::verb::get && request.method() != http::verb::head) {
        return MakeBadRequest(request, "Invalid method");
    }

    std::string_view target = request.target();
    std::size_t start = 0;
    std::size_t max_items = 100;

    // Parse url if it has the parameters "start" and "maxItems"
    if (const auto qpos = target.find('?'); qpos != std::string_view::npos) {
        std::string_view query = target.substr(qpos + 1);
        while (!query.empty()) {
            auto amp = query.find('&');
            std::string_view chunk = (amp == std::string_view::npos) ? query : query.substr(0, amp);
            query = (amp == std::string_view::npos) ? std::string_view{} : query.substr(amp + 1);

            if (chunk.empty()) {
                continue;
            }

            auto eq = chunk.find('=');
            if (eq == std::string_view::npos) {
                continue;
            }

            std::string_view key = chunk.substr(0, eq);
            std::string_view val = chunk.substr(eq + 1);

            if (key == "start") {
                auto parsed = ParseSize(val);
                if (!parsed) {
                    return MakeInvalidArgument(request, "start must be a non-negative integer");
                }
                start = *parsed;
            }
            else if (key == "maxItems") {
                auto parsed = ParseSize(val);
                if (!parsed) {
                    return MakeInvalidArgument(request, "maxItems must be a non-negative integer");
                }
                max_items = *parsed;
            }
        }
    }

    if (max_items > 100) {
        return MakeInvalidArgument(request, "maxItems must be <= 100");
    }

    const auto records = app_.GetPlayerRecords(start, max_items);
    json::array arr;
    arr.reserve(records.size());
    for (const auto& rec : records) {
        json::object obj;
        obj["name"] = rec.name;
        obj["score"] = rec.score;
        obj["playTime"] = rec.play_time;
        arr.emplace_back(std::move(obj));
    }

    const std::string body = json::serialize(arr);
    auto resp = MakeStringResponse(http::status::ok, body, request.version(), request.keep_alive(), ContentType::JSON);
    resp.set(http::field::cache_control, "no-cache");
    if (request.method() == http::verb::head) {
        resp.body().clear();
        resp.content_length(body.size());
    }
    return resp;
}

StringResponse ApiHandler::HandleMovePlayer(const StringRequest& request) {
    // check method
    if (request.method() != http::verb::post) {
        StringResponse res = MakeErrorResponse(http::status::method_not_allowed,
            "invalidMethod", "Only POST method is expected", request);
        res.set(http::field::allow, "POST");
        return res;
    }

    // check content-type
    auto content_type_it = request.find(http::field::content_type);
    if (content_type_it == request.end() || content_type_it->value() != ContentType::JSON) {
        return MakeInvalidArgument(request, "Content-Type must be application/json");
    }

    application::Player::Id player_id;
    auto check_token_res = CheckAuthorizationAndToken(request, player_id);
    if (check_token_res) {
        return *check_token_res;
    }

    // parse body
    json::value body_json;
    try {
        body_json = json::parse(request.body());

        if (!body_json.is_object()) {
            throw std::invalid_argument("");
        }

        const auto& obj = body_json.as_object();

        const auto* user_move_dir_it = obj.if_contains("move");
        if (!user_move_dir_it || !user_move_dir_it->is_string()) {
            throw std::invalid_argument("");
        }

        const std::string move = user_move_dir_it->as_string().c_str();
        if (move.empty()) {
            app_.StopPlayer(player_id);
        } 
        else {
            app_.MovePlayer(player_id, LetterToDirection(move));
        }

    } 
    catch (const std::exception&) {
        return MakeInvalidArgument(request, "Failed to parse action");
    }

    return MakeStringResponse(http::status::ok, "{}", request.version(), request.keep_alive());
}

StringResponse ApiHandler::HandleTick(const StringRequest& request) {
    // check method
    if (request.method() != http::verb::post) {
        StringResponse res = MakeErrorResponse(http::status::method_not_allowed,
            "invalidMethod", "Only POST method is expected", request);
        res.set(http::field::allow, "POST");
        return res;
    }

    // check content-type
    auto content_type_it = request.find(http::field::content_type);
    if (content_type_it == request.end() || content_type_it->value() != ContentType::JSON) {
        return MakeInvalidArgument(request, "Content-Type must be application/json");
    }

    // parse body
    json::value body_json;
    try {
        body_json = json::parse(request.body());

        if (!body_json.is_object()) {
            throw std::invalid_argument("");
        }

        const auto& obj = body_json.as_object();
        const auto* ticks_it = obj.if_contains("timeDelta");
        if (!ticks_it || !(ticks_it->is_int64())) {
            throw std::invalid_argument("");
        }

        int ticks = static_cast<int>(ticks_it->as_int64());
        if (ticks <= 0) {
            throw std::invalid_argument("");
        }
        app_.Tick(static_cast<std::chrono::milliseconds>(ticks));

    }
    catch (const std::exception&) {        
        return MakeInvalidArgument(request, "Failed to parse tick request JSON");
    }

    return MakeStringResponse(http::status::ok, "{}", request.version(), request.keep_alive());
}

StringResponse ApiHandler::HandleMapsEndpoint(const StringRequest& req, PathIt it, PathIt end) const {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        StringResponse res = MakeErrorResponse(http::status::method_not_allowed,
            "invalidMethod", "Invalid method", req);
        res.set(http::field::allow, "GET, HEAD");
        return res;
    }

    // /api/v1/maps
    if (it == end) {
        auto text = json::serialize(GetAllMapsAsJsonArray());
        auto res = MakeStringResponse(http::status::ok, text, req.version(), req.keep_alive());

        if (req.method() == http::verb::head) {
            res.body().clear();
        }
        return res;
    }

    // /api/v1/maps/<id>
    model::Map::Id id(it->string());
    const model::Map* map_ptr = app_.FindMapByMapId(id);
    if (!map_ptr) {
        return MakeErrorResponse(http::status::not_found,
            "mapNotFound", "Map not found", req);
    }

    auto text = json::serialize(config::GetMapAsJsonObject(map_ptr, loot_metadata_));
    auto res = MakeStringResponse(http::status::ok, text, req.version(), req.keep_alive());

    if (req.method() == http::verb::head) {
        res.body().clear();
    }
    return res;
}

StringResponse ApiHandler::HandleGameEndpoint(const StringRequest& req, PathIt it, PathIt end) {
    if (it == end) {
        return MakeBadRequest(req, "Bad Request");
    }

    if (*it == "tick") {
        if (auto_tick_enabled_) {
            return MakeBadRequest(req, "Invalid endpoint");
        }
        return HandleTick(req);
    }

    if (*it == "join") {
        return HandleJoinGame(req);
    }

    if (*it == "players") {
        return HandleGetPlayers(req);
    }

    if (*it == "state") {
        return HandleGetGameState(req);
    }

    if (*it == "records") {
        return HandleGetRecords(req);
    }

    if (*it == "player") {
        ++it;
        if (it == end) {
            return MakeBadRequest(req, "Bad Request");
        }
        if (*it == "action") {
            return HandleMovePlayer(req);
        }
        return MakeBadRequest(req, "Bad Request");
    }

    return MakeBadRequest(req, "Bad Request");
}

StringResponse ApiHandler::HandleRequest(const StringRequest& req) {

    fs::path url;
    try {
        url = MakePathFromTarget(req);
    }
    catch (...) {
        return MakeErrorResponse(http::status::bad_request, 
                                "badRequest", "URL is not correct", req, ContentType::TEXT_PLAIN);
    }

    auto it = url.begin();
    auto end = url.end();

    // /api/...
    ++it;
    if (it == end || *it != "v1") {
        return MakeBadRequest(req, "Bad Request");
    }

    // /api/v1/...
    ++it;
    if (it == end) {
        return MakeBadRequest(req, "Bad Request");
    }

    if (*it == "maps") {
        ++it;
        return HandleMapsEndpoint(req, it, end);
    }

    if (*it == "game") {
        ++it;
        return HandleGameEndpoint(req, it, end);
    }

    return MakeBadRequest(req, "Bad Request");
}

} // namespace http_handler