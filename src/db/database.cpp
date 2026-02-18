#include "db/database.hpp"
#include "db/sql_helpers.hpp"
#include "paths.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <system_error>

namespace db {

static constexpr const char* kAppName = "intrinsic";
static constexpr const char* kDbFileName = "intrinsic.db";

static std::string sqlite_path_utf8(const std::filesystem::path& path)
{
    return path.string();
}

static constexpr const char* kSchemaSQL = R"SQL(
CREATE TABLE IF NOT EXISTS tickers (
    ticker      TEXT    PRIMARY KEY,
    last_update INTEGER NOT NULL
) WITHOUT ROWID;

CREATE INDEX IF NOT EXISTS idx_tickers_order ON tickers(last_update DESC, ticker ASC);

CREATE TABLE IF NOT EXISTS finances (
    ticker                      TEXT    NOT NULL,
    year                        INTEGER NOT NULL,
    period_type                 TEXT    NOT NULL,
    current_assets              INTEGER,
    non_current_assets          INTEGER,
    eps                         REAL,
    cash_and_equivalents        INTEGER,
    cash_flow_from_financing    INTEGER,
    cash_flow_from_investing    INTEGER,
    cash_flow_from_operations   INTEGER,
    revenue                     INTEGER,
    current_liabilities         INTEGER,
    non_current_liabilities     INTEGER,
    net_income                  INTEGER,
    PRIMARY KEY (ticker, year, period_type),
    FOREIGN KEY (ticker) REFERENCES tickers(ticker) ON DELETE CASCADE
) WITHOUT ROWID;
)SQL";

Database::Database() = default;

void Database::close()
{
    if (db_) {
        const int rc = sqlite3_close(db_);
        if (rc != SQLITE_OK) sqlite3_close_v2(db_);
        db_ = nullptr;
    }

    db_path_.clear();
}

Database::~Database()
{
    close();
}

std::filesystem::path Database::default_db_path_()
{
    std::string err;
    const auto base = intrinsic::platform::data_home(&err);
    if (base.empty()) {
        throw std::runtime_error(err);
    }
    return base / kAppName / kDbFileName;
}

void Database::ensure_parent_dir_exists_(const std::filesystem::path& file_path)
{
    const auto dir = file_path.parent_path();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw std::runtime_error("failed to create db directory '" +
                                 dir.string() + "': " + ec.message());
    }
}

void Database::open_connection_(const std::filesystem::path& file_path)
{
    if (db_) return; // already open

    sqlite3* tmp = nullptr;
    const std::string utf8_path = sqlite_path_utf8(file_path);

    const int flags =
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int rc = sqlite3_open_v2(utf8_path.c_str(), &tmp, flags, nullptr);

    if (rc != SQLITE_OK) {
        std::string msg = tmp ? sqlite3_errmsg(tmp) : "sqlite open error";
        if (tmp) sqlite3_close(tmp);
        throw std::runtime_error("failed to open sqlite db at: " +
                                 file_path.string() + " (" + msg + ")");
    }

    sqlite3_busy_timeout(tmp, 5000);

    db::detail::exec_sql(tmp, "PRAGMA foreign_keys = ON;");
    db::detail::exec_sql(tmp, "PRAGMA journal_mode = WAL;");
    db::detail::exec_sql(tmp, "PRAGMA synchronous = NORMAL;");
    db::detail::exec_sql(tmp, "PRAGMA temp_store = MEMORY;");
    db::detail::exec_sql(tmp, "PRAGMA cache_size = -2000;"); // 2000 KB
    db::detail::exec_sql(tmp, "PRAGMA wal_autocheckpoint = 1000;");

    db_ = tmp;
    db_path_ = file_path;
}

void Database::apply_schema_()
{
    db::detail::exec_sql(db_, "BEGIN;");
    try {
        db::detail::exec_sql(db_, kSchemaSQL);
        db::detail::exec_sql(db_, "COMMIT;");
    }
    catch (...) {
        db::detail::exec_sql(db_, "ROLLBACK;");
        throw;
    }
}

void Database::open_or_create()
{
    if (db_) return;

    const auto file_path = default_db_path_();
    ensure_parent_dir_exists_(file_path);

    open_connection_(file_path);

    apply_schema_(); // IF NOT EXISTS handles it
}

} // namespace db

