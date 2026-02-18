#pragma once
#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace db::detail {

inline void exec_sql(sqlite3* db, const char* sql)
{
    char* err = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "sqlite exec error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

[[noreturn]] inline void throw_sqlite(sqlite3* db, const char* ctx)
{
    throw std::runtime_error(std::string(ctx) + ": " + sqlite3_errmsg(db));
}

} // namespace db::detail


