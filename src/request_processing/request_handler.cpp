#include "request_handler.h"
#include "make_response.h"
#include "path_handler.h"

namespace {
    namespace json = boost::json;
    namespace fs = std::filesystem;
    using namespace http_handler;

    std::string_view DetectMime(const fs::path& path) {
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        struct ExtMime {
            std::string_view ext;
            std::string_view mime;
        };

        static constexpr ExtMime table[] = {
            {".htm",  ContentType::TEXT_HTML},
            {".html", ContentType::TEXT_HTML},
            {".css",  ContentType::TEXT_CSS},
            {".txt",  ContentType::TEXT_PLAIN},
            {".js",   ContentType::TEXT_JAVA},
            {".json", ContentType::JSON},
            {".png",  ContentType::IMAGE_PNG},
            {".jpg",  ContentType::IMAGE_JPEG},
            {".jpeg", ContentType::IMAGE_JPEG},
            {".gif",  ContentType::IMAGE_GIF},
            {".bmp",  ContentType::IMAGE_BMP},
            {".ico",  ContentType::IMAGE_ICO},
            {".svg",  ContentType::IMAGE_SVG},
            {".svgz", ContentType::IMAGE_SVG},
            {".mp3",  ContentType::AUDIO_MP3},
        };

        for (const auto& [e, m] : table) {
            if (ext == e) {
                return m;
            }
        }

        return ContentType::UNKNOWN;
    }

} // namespace

namespace http_handler {

    namespace json = boost::json;
    namespace fs = std::filesystem;      

    VariantResponse RequestHandler::HandleFileRequest(const StringRequest& req) {
        fs::path url;
        try {
            url = MakePathFromTarget(req);
        }
        catch (...) {
            return MakeErrorResponse(http::status::bad_request, 
                                    "badRequest", "URL is not correct", req, ContentType::TEXT_PLAIN);
        }

        fs::path abs_path = fs::weakly_canonical(root_path_ / url);
        if (!IsSubPath(abs_path, root_path_)) {
            return MakeErrorResponse(http::status::bad_request, 
                                    "badRequest", "Bad Request", req, ContentType::TEXT_PLAIN);
        }

        if (!fs::exists(abs_path)) {
            return MakeErrorResponse(http::status::not_found, 
                                    "fileNotFound", "File not found", req, ContentType::TEXT_PLAIN);
        }

        if (fs::is_directory(abs_path)) {
            abs_path = fs::weakly_canonical(root_path_ / "index.html");
        }

        switch (req.method()) {
            // return the head as string response
            case http::verb::head: {
                const auto file_size = fs::file_size(abs_path);
                StringResponse res = MakeStringResponse(http::status::ok, "", req.version(), req.keep_alive(), DetectMime(abs_path));
                res.content_length(file_size);
                return res;
            }
            // return a file
            case http::verb::get: {
                FileResponse res{http::status::ok, req.version()};
                res.keep_alive(req.keep_alive());
                res.set(http::field::content_type, DetectMime(abs_path));

                http::file_body::value_type file;
                boost::system::error_code ec;
                // тут string().c_str() делается для совместимости с Windows, 
                // т.к. для Windows fs::path.c_str() вернёт wchar_t,
                // а file.open ожидает char const*, т.е. другой тип
                file.open(abs_path.string().c_str(), beast::file_mode::read, ec);
                if (ec) {
                    return MakeErrorResponse(http::status::internal_server_error,
                                     "internalError", "Failed to open file", req, ContentType::TEXT_PLAIN);
                }

                res.body() = std::move(file);
                res.prepare_payload();

                return res;
            }

            default: {
                StringResponse res = MakeErrorResponse(http::status::method_not_allowed, 
                                        "methodNotAllowed", "This method is not allowed", req);
                res.set(http::field::allow, "GET, HEAD");
                return res;
            }
                
        } 
    }

}  // namespace http_handler
