#pragma once

#include <mutex>
#include <string>

struct sqlite3;

namespace projection::server::db {

class SqliteConnection {
public:
    SqliteConnection();
    ~SqliteConnection();

    void open(const std::string& path);

    sqlite3* getHandle() const;

    void execute(const std::string& sql) const;

    std::unique_lock<std::recursive_mutex> lock() const;

private:
    sqlite3* handle_;
    mutable std::recursive_mutex mutex_;
};

}  // namespace projection::server::db
