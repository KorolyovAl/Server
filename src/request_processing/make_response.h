#pragma once

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/beast.hpp>
#include <boost/json.hpp>

#include <string_view>

namespace http_handler {

namespace json = boost::json;
namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;
using StringRequest = http::request<http::string_body>;

using namespace std::string_view_literals;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view TEXT_CSS = "text/css"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view TEXT_JAVA = "text/javascript"sv;

    constexpr static std::string_view JSON = "application/json"sv;
    constexpr static std::string_view XML = "application/xml"sv;

    constexpr static std::string_view IMAGE_PNG = "image/png"sv;
    constexpr static std::string_view IMAGE_JPEG = "image/jpeg"sv;
    constexpr static std::string_view IMAGE_GIF = "image/gif"sv;
    constexpr static std::string_view IMAGE_BMP = "image/bmp"sv;
    constexpr static std::string_view IMAGE_ICO = "image/vnd.microsoft.icon"sv;
    constexpr static std::string_view IMAGE_TIFF = "image/tiff"sv;
    constexpr static std::string_view IMAGE_SVG = "image/svg+xml"sv;

    constexpr static std::string_view AUDIO_MP3 = "audio/mpeg"sv;

    constexpr static std::string_view UNKNOWN = "application/octet-stream"sv;
};

inline StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                bool keep_alive, std::string_view content_type = ContentType::JSON) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);

    if (content_type == ContentType::JSON) {
        response.set(http::field::cache_control, "no-cache");
    }

    return response; 
}

inline StringResponse MakeErrorResponse(http::status status, std::string code, std::string message, 
                                const StringRequest& req, std::string_view content_type = ContentType::JSON) {
    json::object bad_response;
    bad_response["code"] = code;
    bad_response["message"] = message;
    std::string text = json::serialize(bad_response);

    return MakeStringResponse(status, text, req.version(), req.keep_alive(), content_type);
}

inline StringResponse MakeBadRequest(const StringRequest& req, std::string message) {  
    return MakeErrorResponse(http::status::bad_request, "badRequest", std::move(message), req);
}

inline StringResponse MakeInvalidArgument(const StringRequest& req, std::string message) {  
    return MakeErrorResponse(http::status::bad_request, "invalidArgument", std::move(message), req);
}

} // namespace http_handler