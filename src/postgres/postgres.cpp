#include "postgres.h"

#include <pqxx/pqxx>

#include <stdexcept>

namespace postgres {

namespace {

constexpr const char* kCreateTable = R"(
    CREATE TABLE IF NOT EXISTS retired_players (
        id SERIAL PRIMARY KEY,
        name TEXT NOT NULL,
        score INTEGER NOT NULL,
        play_time DOUBLE PRECISION NOT NULL
    );
)";

constexpr const char* kCreateIndex = R"(
    CREATE INDEX IF NOT EXISTS retired_players_sort_idx
    ON retired_players (score DESC, play_time ASC, name ASC);
)";

int EnsureSchema(const std::string& db_url) {
    pqxx::connection conn{db_url};
    pqxx::work tr{conn};

    tr.exec(kCreateTable);
    tr.exec(kCreateIndex);
    tr.commit();
    return 0;
}

}  // namespace

void RecordsRepositoryImpl::AddRecord(const application::PlayerRecord& record) {
    tr_.exec_prepared("insert_retired_player", record.name, record.score, record.play_time);
}
 
std::vector<application::PlayerRecord> RecordsRepositoryImpl::GetRecords(std::size_t start, std::size_t max_items) {
    pqxx::result r = tr_.exec_prepared("select_records", start, max_items);

    std::vector<application::PlayerRecord> result;
    result.reserve(r.size());
    for (const auto& row : r) {
        application::PlayerRecord rec;
        rec.name = row[0].c_str();
        rec.score = row[1].as<int>();
        rec.play_time = row[2].as<double>();
        result.push_back(std::move(rec));
    }
    return result;
}

UnitOfWorkImpl::UnitOfWorkImpl(ConnectionPool::ConnectionWrapper conn)
    : conn_{std::move(conn)}
    , tr_{*conn_} {
}

application::RecordsRepository& UnitOfWorkImpl::GetRecords() {
    return records_;
}

void UnitOfWorkImpl::Commit() {
    if (!committed_) {
        tr_.commit();
        committed_ = true;
    }
}

std::unique_ptr<application::UnitOfWork> UnitOfWorkFactoryImpl::Create() {
    return std::make_unique<UnitOfWorkImpl>(pool_.GetConnection());
}

Database::Database(std::string db_url, std::size_t pool_capacity)
    : pool_{pool_capacity > 0 ? pool_capacity : 1, [db_url, schema_ensured = EnsureSchema(db_url)] {
        (void)schema_ensured;
        auto conn = std::make_shared<pqxx::connection>(db_url);
        conn->prepare("insert_retired_player",
                      "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);");
        conn->prepare("select_records",
                      "SELECT name, score, play_time FROM retired_players "
                      "ORDER BY score DESC, play_time ASC, name ASC "
                      "OFFSET $1 LIMIT $2;");
        return conn;
    }} {
}

}  // namespace postgres
