#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace application {

struct PlayerRecord {
    std::string name;
    int score = 0;
    double play_time = 0.0;
};

class RecordsRepository {
public:
    virtual void AddRecord(const PlayerRecord& record) = 0;
    virtual std::vector<PlayerRecord> GetRecords(std::size_t start, std::size_t max_items) = 0;

    virtual ~RecordsRepository() = default;
};

}  // namespace application
