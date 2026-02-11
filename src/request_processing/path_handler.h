#pragma once

#include <filesystem>
#include <string>

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/beast.hpp>

namespace http_handler {

namespace fs = std::filesystem;
namespace beast = boost::beast;
namespace http = beast::http;
using StringRequest = http::request<http::string_body>;

inline fs::path MakePathFromTarget(const StringRequest& req) {
    std::string_view target = req.target();

    // Drop URL query part, if any.
    // Query parameters are handled separately by API handlers.
    if (const auto pos = target.find('?'); pos != std::string_view::npos) {
        target = target.substr(0, pos);
    }

    // remove "/"
    if (!target.empty() && target.front() == '/') {
        target.remove_prefix(1);
    }

    // decode URL
    std::string res;
    res.reserve(target.size());

    auto SymbToInt = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }

        if (c >= 'a' && c <= 'f') {
            return 10 + (c - 'a');
        }

        if (c >= 'A' && c <= 'F') {
            return 10 + (c - 'A');
        }

        return -1;
    };

    for (std::size_t i = 0; i < target.size(); ++i) {
        char ch = target[i];

        if (ch == '%') {
            if (i + 2 >= target.size()) {
                throw std::runtime_error("bad percent-encoding");
            }

            int first = SymbToInt(target[i + 1]);
            int second = SymbToInt(target[i + 2]);

            if (first < 0 || second < 0) {
                throw std::runtime_error("bad percent-encoding");
            }

            char decoded = static_cast<char>((first << 4) | second);
            res.push_back(decoded);
            i += 2;
        }
        else if (ch == '+') {
            res.push_back(' ');
        }
        else {
            res.push_back(ch);
        }
    }

    return fs::path(res);
}

// return true if path is subpath of base_path
inline bool IsSubPath(fs::path path, fs::path base) {
    // bringing the path to the canonical form (without . and ..)
    path = fs::weakly_canonical(path);

    // check that all components of base are contained within path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

} // namespace http_handler