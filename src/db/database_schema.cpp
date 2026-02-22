#include "db/database.hpp"
#include "db/sql_helpers.hpp"

#include <string>
#include <string_view>

namespace db {

static constexpr const char* kSchemaSQL = R"SQL(
CREATE TABLE IF NOT EXISTS tickers (
    ticker      TEXT    PRIMARY KEY,
    last_update INTEGER NOT NULL,
    portfolio   INTEGER NOT NULL DEFAULT 0
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

static bool table_has_column(sqlite3* db,
                             const char* table_name,
                             std::string_view column_name)
{
    const std::string sql = "PRAGMA table_info(" + std::string(table_name) +
                            ");";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        db::detail::throw_sqlite(db, "prepare table_info failed");
    }

    bool found = false;
    while (true) {
        const int rc = sqlite3_step(st);
        if (rc == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(st, 1);
            if (!text) continue;
            const std::string_view name{
                reinterpret_cast<const char*>(text),
            };
            if (name == column_name) {
                found = true;
                break;
            }
            continue;
        }
        if (rc == SQLITE_DONE) break;
        sqlite3_finalize(st);
        db::detail::throw_sqlite(db, "table_info step failed");
    }

    sqlite3_finalize(st);
    return found;
}

static void ensure_tickers_portfolio_column(sqlite3* db)
{
    if (table_has_column(db, "tickers", "portfolio")) return;
    db::detail::exec_sql(
        db,
        "ALTER TABLE tickers ADD COLUMN portfolio INTEGER NOT NULL DEFAULT 0;");
}

void Database::apply_schema_()
{
    db::detail::exec_sql(db_, "BEGIN;");
    try {
        db::detail::exec_sql(db_, kSchemaSQL);
        ensure_tickers_portfolio_column(db_);
        db::detail::exec_sql(
            db_,
            "CREATE INDEX IF NOT EXISTS idx_tickers_portfolio "
            "ON tickers(portfolio, last_update DESC, ticker ASC);");
        db::detail::exec_sql(db_, "COMMIT;");
    }
    catch (...) {
        db::detail::exec_sql(db_, "ROLLBACK;");
        throw;
    }
}

} // namespace db
