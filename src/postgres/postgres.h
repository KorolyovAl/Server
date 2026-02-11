#pragma once

#include <pqxx/connection>
#include <pqxx/transaction>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "connection_pool.h"

#include "../app/records_repository.h"
#include "../app/unit_of_work.h"

namespace postgres {

class RecordsRepositoryImpl : public application::RecordsRepository {
public:
    explicit RecordsRepositoryImpl(pqxx::transaction_base& tr)
        : tr_{tr} {
    }

    void AddRecord(const application::PlayerRecord& record) override;
    std::vector<application::PlayerRecord> GetRecords(std::size_t start, std::size_t max_items) override;

private:
    pqxx::transaction_base& tr_;
};

class UnitOfWorkImpl : public application::UnitOfWork {
public:
    explicit UnitOfWorkImpl(ConnectionPool::ConnectionWrapper conn);

    application::RecordsRepository& GetRecords() override;
    void Commit() override;

private:
    ConnectionPool::ConnectionWrapper conn_;
    pqxx::work tr_;
    RecordsRepositoryImpl records_{tr_};
    bool committed_ = false;
};
 
class UnitOfWorkFactoryImpl : public application::UnitOfWorkFactory {
public:
    explicit UnitOfWorkFactoryImpl(ConnectionPool& pool)
        : pool_{pool} {
    }

    std::unique_ptr<application::UnitOfWork> Create() override;

private:
    ConnectionPool& pool_;
};

class Database {
public:
    Database(std::string db_url, std::size_t pool_capacity);

    application::UnitOfWorkFactory& GetUnitOfWorkFactory() & {
        return uow_factory_;
    }

private:
    ConnectionPool pool_;
    UnitOfWorkFactoryImpl uow_factory_{pool_};
};

}  // namespace postgres
