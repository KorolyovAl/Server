#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <utility>

#include "../app/application.h"
#include "server_state.h"

namespace infrastructure {
 
class SerializingListener {
public:
    using ms = std::chrono::milliseconds;

    SerializingListener(std::string path,
                        ServerState& server_state,
                        application::Application& app,
                        std::optional<ms> save_interval)
        : app_{app}
        , server_state_{server_state}
        , save_interval_{save_interval}
        , time_since_last_save_{0}
        , save_path_{std::move(path)} {
    }

    void OnTick(ms delta) {
        if (!save_interval_.has_value()) {
            return;
        }

        time_since_last_save_ += delta;

        if (time_since_last_save_ >= *save_interval_) {
            SaveNow();
            time_since_last_save_ = ms{0};
        }
    }

    void SaveNow() {
        if (save_path_.empty()) {
            return;
        }

        server_state_.Save(app_.GetState(), save_path_);
    }

private:
    application::Application& app_;
    ServerState& server_state_;
    std::optional<ms> save_interval_;
    ms time_since_last_save_;
    std::string save_path_;
};

} // namespace infrastructure
