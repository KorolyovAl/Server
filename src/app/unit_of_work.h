#pragma once

#include <memory>

#include "records_repository.h"

namespace application {

// UnitOfWork represents a single database transaction.
// Repositories returned by this object operate within the same transaction.
class UnitOfWork {
public:
    virtual RecordsRepository& GetRecords() = 0;
    virtual void Commit() = 0;

    virtual ~UnitOfWork() = default;
};

class UnitOfWorkFactory {
public:
    virtual std::unique_ptr<UnitOfWork> Create() = 0;

    virtual ~UnitOfWorkFactory() = default;
};

}  // namespace application
