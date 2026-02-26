#include "db/database.hpp"
#include "db/sql_helpers.hpp"

#include <string>
#include <string_view>

namespace db {

static constexpr const char* kSchemaSQL = R"SQL(
CREATE TABLE IF NOT EXISTS tickers (
    ticker      TEXT    PRIMARY KEY,
    last_update INTEGER NOT NULL,
    portfolio   INTEGER NOT NULL DEFAULT 0,
    type        INTEGER NOT NULL DEFAULT 1
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
    total_loans                 INTEGER,
    goodwill                    INTEGER,
    total_assets                INTEGER,
    total_deposits              INTEGER,
    total_liabilities           INTEGER,
    net_interest_income         INTEGER,
    non_interest_income         INTEGER,
    loan_loss_provisions        INTEGER,
    non_interest_expense        INTEGER,
    risk_weighted_assets        INTEGER,
    common_equity_tier1         INTEGER,
    net_charge_offs             INTEGER,
    non_performing_loans        INTEGER,
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

static void ensure_column_exists(sqlite3* db,
                                 const char* table_name,
                                 const char* column_name,
                                 const char* definition_sql)
{
    if (table_has_column(db, table_name, column_name)) return;

    const std::string sql = "ALTER TABLE " + std::string(table_name) +
                            " ADD COLUMN " + std::string(definition_sql) + ";";
    db::detail::exec_sql(db, sql.c_str());
}

static void ensure_tickers_portfolio_column(sqlite3* db)
{
    ensure_column_exists(
        db, "tickers", "portfolio", "portfolio INTEGER NOT NULL DEFAULT 0");
}

static void ensure_tickers_type_column(sqlite3* db)
{
    ensure_column_exists(db, "tickers", "type", "type INTEGER NOT NULL DEFAULT 1");
}

static void ensure_finances_bank_columns(sqlite3* db)
{
    ensure_column_exists(db, "finances", "total_loans", "total_loans INTEGER");
    ensure_column_exists(db, "finances", "goodwill", "goodwill INTEGER");
    ensure_column_exists(db, "finances", "total_assets", "total_assets INTEGER");
    ensure_column_exists(
        db, "finances", "total_deposits", "total_deposits INTEGER");
    ensure_column_exists(
        db, "finances", "total_liabilities", "total_liabilities INTEGER");

    ensure_column_exists(
        db, "finances", "net_interest_income", "net_interest_income INTEGER");
    ensure_column_exists(
        db, "finances", "non_interest_income", "non_interest_income INTEGER");
    ensure_column_exists(db,
                         "finances",
                         "loan_loss_provisions",
                         "loan_loss_provisions INTEGER");
    ensure_column_exists(db,
                         "finances",
                         "non_interest_expense",
                         "non_interest_expense INTEGER");

    ensure_column_exists(
        db, "finances", "risk_weighted_assets", "risk_weighted_assets INTEGER");
    ensure_column_exists(
        db, "finances", "common_equity_tier1", "common_equity_tier1 INTEGER");

    ensure_column_exists(
        db, "finances", "net_charge_offs", "net_charge_offs INTEGER");
    ensure_column_exists(
        db, "finances", "non_performing_loans", "non_performing_loans INTEGER");
}

void Database::apply_schema_()
{
    db::detail::exec_sql(db_, "BEGIN;");
    try {
        db::detail::exec_sql(db_, kSchemaSQL);
        ensure_tickers_portfolio_column(db_);
        ensure_tickers_type_column(db_);
        ensure_finances_bank_columns(db_);
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
