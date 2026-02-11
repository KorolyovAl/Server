#pragma once
#include "../server/http_server.h"
#include "../app/application.h"
#include "../metadata/loot_data.h"
#include "api_handler.h"

#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/dispatch.hpp>

#include <filesystem>
#include <string_view>
#include <variant>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

using namespace std::literals;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using VariantResponse = std::variant<StringResponse, FileResponse>;

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit RequestHandler(application::Application& application, 
                            const metadata::LootMetaPerMap& loot_metadata,
                            const std::filesystem::path& root_path, 
                            Strand strand, bool auto_tick_enabled)
        : application_{application}
        , loot_metadata_(loot_metadata)
        , root_path_{std::filesystem::weakly_canonical(root_path)}
        , api_handler_{application, loot_metadata, auto_tick_enabled}
        , strand_{strand} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const auto target = req.target();
        const bool is_api = target.size() >= 4 && target.substr(0, 4) == "/api"sv;

        if (is_api) {
            // Dispatch to the strand to ensure thread safety
            net::dispatch(strand_,
                [self = shared_from_this(), req = std::move(req), send = std::forward<Send>(send)]() mutable {
                    VariantResponse resp = self->api_handler_.HandleRequest(std::move(req));

                    std::visit(
                        [&send](auto&& concrete_resp) {
                            send(std::move(concrete_resp));
                        },
                        std::move(resp)
                    );
                }
            );
        }
        else {
            VariantResponse resp = HandleFileRequest(std::move(req));

            std::visit(
                [&send](auto&& concrete_resp) {
                    send(std::move(concrete_resp));
                },
                std::move(resp)
            );
        }
    }

private:
    VariantResponse HandleFileRequest(const StringRequest& request);

private:
    application::Application& application_;
    const std::filesystem::path root_path_;
    ApiHandler api_handler_;
    const metadata::LootMetaPerMap& loot_metadata_;

    Strand strand_;
};

}  // namespace http_handler
