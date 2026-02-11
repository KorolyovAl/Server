#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "../app/app_state.h"
 
namespace infrastructure {

class ServerState {
public:
    application::AppState Load(std::string_view path) const {
        std::ifstream in(std::string(path), std::ios::binary);
        if (!in.is_open()) {
            throw std::runtime_error("state file open failed");
        }

        application::AppState state;
        boost::archive::text_iarchive ar(in);
        ar >> state;
        return state;
    }

    void Save(const application::AppState& app_state, std::string_view path) const {
        namespace fs = std::filesystem;

        const fs::path dst{std::string(path)};
        const fs::path tmp = dst.string() + ".tmp";

        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                throw std::runtime_error("state temp file open failed");
            }

            boost::archive::text_oarchive ar(out);
            ar << app_state;
        }

        fs::rename(tmp, dst);
    }
};

} // namespace infrastructure