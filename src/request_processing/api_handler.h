#pragma once

#include "../app/application.h"
#include "../server/http_server.h"
#include "../metadata/loot_data.h"

#include "make_response.h"

#include <optional>
#include <filesystem>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

class ApiHandler{
public:
    explicit ApiHandler(application::Application& app, 
                        const metadata::LootMetaPerMap& loot_metadata, 
                        bool auto_tick_enabled)
        : app_{app}
        , loot_metadata_(loot_metadata)
        , auto_tick_enabled_{auto_tick_enabled} {
    }

    StringResponse HandleRequest(const StringRequest& req);

private:
    using PathIt = std::filesystem::path::const_iterator;

    json::array GetAllMapsAsJsonArray() const;

    StringResponse HandleMapsEndpoint(const StringRequest& req, PathIt it, PathIt end) const;
    StringResponse HandleGameEndpoint(const StringRequest& req, PathIt it, PathIt end);

    StringResponse HandleJoinGame(const StringRequest& request);
    StringResponse HandleGetPlayers(const StringRequest& request) const;
    StringResponse HandleGetGameState(const StringRequest& request) const;
    StringResponse HandleMovePlayer(const StringRequest& request);
    StringResponse HandleTick(const StringRequest& request);
    StringResponse HandleGetRecords(const StringRequest& request) const;

    std::optional<StringResponse> CheckAuthorizationAndToken(const StringRequest& request, application::Player::Id& player_id) const;

private:
    application::Application& app_;
    const metadata::LootMetaPerMap& loot_metadata_;
    bool auto_tick_enabled_ = false; 
};

}