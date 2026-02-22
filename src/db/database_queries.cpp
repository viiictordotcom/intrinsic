#include "db/database.hpp"
#include "db/sql_helpers.hpp"

#include <ctime>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <cstdint>
#include <optional>
#include <vector>

namespace db {

// *
// **
// ***
// ****
// ***** HELPERS

class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) : db_(db)
    {
        if (sqlite3_prepare_v2(db_, sql, -1, &st_, nullptr) != SQLITE_OK) {
            db::detail::throw_sqlite(db_, "prepare failed");
        }
    }
    ~Stmt()
    {
        if (st_) sqlite3_finalize(st_);
    }

    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const { return st_; }

private:
    sqlite3* db_;
    sqlite3_stmt* st_{nullptr};
};

static std::string col_text(sqlite3_stmt* st, int i)
{
    const unsigned char* t = sqlite3_column_text(st, i);
    return t ? reinterpret_cast<const char*>(t) : "";
}

static std::optional<std::int64_t> col_i64_opt(sqlite3_stmt* st, int i)
{
    if (sqlite3_column_type(st, i) == SQLITE_NULL) return std::nullopt;
    return sqlite3_column_int64(st, i);
}

static std::optional<double> col_f64_opt(sqlite3_stmt* st, int i)
{
    if (sqlite3_column_type(st, i) == SQLITE_NULL) return std::nullopt;
    return sqlite3_column_double(st, i);
}

static void
bind_text(sqlite3* db, sqlite3_stmt* st, int idx, const std::string& s)
{
    if (sqlite3_bind_text(st, idx, s.c_str(), -1, SQLITE_TRANSIENT) !=
        SQLITE_OK) {
        db::detail::throw_sqlite(db, "bind_text failed");
    }
}

static void bind_i64_opt(sqlite3* db,
                         sqlite3_stmt* st,
                         int idx,
                         std::optional<std::int64_t> v)
{
    int rc = SQLITE_OK;
    if (!v.has_value())
        rc = sqlite3_bind_null(st, idx);
    else
        rc = sqlite3_bind_int64(st, idx, *v);

    if (rc != SQLITE_OK) db::detail::throw_sqlite(db, "bind_i64_opt failed");
}

static void
bind_f64_opt(sqlite3* db, sqlite3_stmt* st, int idx, std::optional<double> v)
{
    int rc = SQLITE_OK;
    if (!v.has_value())
        rc = sqlite3_bind_null(st, idx);
    else
        rc = sqlite3_bind_double(st, idx, *v);

    if (rc != SQLITE_OK) db::detail::throw_sqlite(db, "bind_f64_opt failed");
}

static std::pair<int, std::string> parse_period(const std::string& period)
{
    // YYYY-<period_type>

    if (period.size() < 6 || period[4] != '-') {
        throw std::runtime_error(
            "invalid period format (expected YYYY-<type>): " + period);
    }

    int year = 0;
    try {
        year = std::stoi(period.substr(0, 4));
    }
    catch (...) {
        throw std::runtime_error("invalid year in period: " + period);
    }

    std::string period_type = period.substr(5);
    if (period_type.empty()) {
        throw std::runtime_error("invalid period_type in period: " + period);
    }

    return {year, std::move(period_type)};
}

template <class Fn>
static bool in_transaction(sqlite3* db, Fn&& fn, std::string* err = nullptr)
{
    try {
        db::detail::exec_sql(db, "BEGIN IMMEDIATE;");
        fn();
        db::detail::exec_sql(db, "COMMIT;");
        return true;
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        try {
            db::detail::exec_sql(db, "ROLLBACK;");
        }
        catch (...) {
        }
        return false;
    }
}

// *
// **
// ***
// ****
// ***** QUERIES

std::vector<db::Database::TickerRow> db::Database::get_tickers(
    int page,
    int page_size,
    TickerSortKey key,
    SortDir dir,
    std::string* err,
    bool portfolio_only)
{
    try {
        if (page < 0) page = 0;
        if (page_size <= 0) page_size = 1;

        const std::int64_t offset = static_cast<std::int64_t>(page) *
                                    static_cast<std::int64_t>(page_size);
        if (offset < 0 || offset > static_cast<std::int64_t>(
                                       std::numeric_limits<int>::max())) {
            throw std::runtime_error("page offset out of range");
        }

        const char* order_by = nullptr;

        if (key == TickerSortKey::LastUpdate) {
            if (dir == SortDir::Desc) {
                order_by = "last_update DESC, ticker ASC";
            }
            else {
                order_by = "last_update ASC, ticker ASC";
            }
        }
        else { // TickerSortKey::Ticker
            if (dir == SortDir::Asc) {
                order_by = "ticker ASC";
            }
            else {
                order_by = "ticker DESC";
            }
        }

        std::string sql = "SELECT ticker, last_update, portfolio "
                          "FROM tickers ";
        if (portfolio_only) {
            sql += "WHERE portfolio = 1 ";
        }

        sql += "ORDER BY " +
                          std::string(order_by) +
                          " "
                          "LIMIT ? OFFSET ?;";

        Stmt st{db_, sql.c_str()};

        if (sqlite3_bind_int(st.get(), 1, page_size) != SQLITE_OK)
            db::detail::throw_sqlite(db_, "bind page_size failed");
        if (sqlite3_bind_int64(st.get(), 2, offset) != SQLITE_OK)
            db::detail::throw_sqlite(db_, "bind offset failed");

        std::vector<TickerRow> out;
        while (true) {
            const int rc = sqlite3_step(st.get());
            if (rc == SQLITE_ROW) {
                TickerRow r;
                r.ticker = col_text(st.get(), 0);
                r.last_update = sqlite3_column_int64(st.get(), 1);
                r.portfolio = sqlite3_column_int(st.get(), 2) != 0;
                out.push_back(std::move(r));
            }
            else if (rc == SQLITE_DONE) {
                break;
            }
            else {
                db::detail::throw_sqlite(db_, "step failed");
            }
        }
        return out;
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return {};
    }
}

std::vector<db::Database::TickerRow> db::Database::search_tickers(
    const std::string& contains, int limit, std::string* err, bool portfolio_only)
{
    try {
        if (limit <= 0) limit = 1;
        if (contains.empty()) return {};

        std::string sql = R"SQL(
            SELECT ticker, last_update, portfolio
            FROM tickers
            WHERE UPPER(ticker) LIKE '%' || UPPER(?) || '%'
        )SQL";
        if (portfolio_only) {
            sql += " AND portfolio = 1 ";
        }
        sql += R"SQL(
            ORDER BY ticker ASC
            LIMIT ?;
        )SQL";

        Stmt st{db_, sql.c_str()};
        bind_text(db_, st.get(), 1, contains);
        if (sqlite3_bind_int(st.get(), 2, limit) != SQLITE_OK)
            db::detail::throw_sqlite(db_, "bind limit failed");

        std::vector<TickerRow> out;
        while (true) {
            const int rc = sqlite3_step(st.get());
            if (rc == SQLITE_ROW) {
                TickerRow r;
                r.ticker = col_text(st.get(), 0);
                r.last_update = sqlite3_column_int64(st.get(), 1);
                r.portfolio = sqlite3_column_int(st.get(), 2) != 0;
                out.push_back(std::move(r));
            }
            else if (rc == SQLITE_DONE) {
                break;
            }
            else {
                db::detail::throw_sqlite(db_, "search tickers step failed");
            }
        }

        return out;
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return {};
    }
}

bool Database::toggle_ticker_portfolio(const std::string& ticker, std::string* err)
{
    try {
        const char* sql = R"SQL(
            UPDATE tickers
            SET portfolio = CASE WHEN portfolio = 0 THEN 1 ELSE 0 END
            WHERE ticker = ?;
        )SQL";

        Stmt st{db_, sql};
        bind_text(db_, st.get(), 1, ticker);

        const int rc = sqlite3_step(st.get());
        if (rc != SQLITE_DONE) {
            db::detail::throw_sqlite(db_, "toggle ticker portfolio step failed");
        }

        return sqlite3_changes(db_) > 0;
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}

bool Database::delete_period(const std::string& ticker,
                             const std::string& period,
                             std::string* err)
{
    try {
        const auto [year, period_type] = parse_period(period);

        return in_transaction(
            db_,
            [&] {
                int count = 0; // how many periods for this ticker?
                {
                    const char* sql = R"SQL(
                        SELECT COUNT(*)
                        FROM finances
                        WHERE ticker = ?;
                    )SQL";

                    Stmt st{db_, sql};
                    bind_text(db_, st.get(), 1, ticker);

                    const int rc = sqlite3_step(st.get());
                    if (rc != SQLITE_ROW)
                        db::detail::throw_sqlite(db_,
                                                 "count finances step failed");

                    count = sqlite3_column_int(st.get(), 0);
                }

                if (count <= 0) {
                    db::detail::throw_sqlite(db_,
                                             "no finances rows for ticker");
                }

                if (count == 1) {
                    // last period -> delete ticker -> cascade on finances
                    const char* sql = R"SQL(
                        DELETE FROM tickers
                        WHERE ticker = ?;
                    )SQL";

                    Stmt st{db_, sql};
                    bind_text(db_, st.get(), 1, ticker);

                    const int rc = sqlite3_step(st.get());
                    if (rc != SQLITE_DONE)
                        db::detail::throw_sqlite(db_,
                                                 "delete ticker step failed");
                }
                else {
                    // not last -> delete only on finances
                    const char* sql = R"SQL(
                        DELETE FROM finances
                        WHERE ticker = ?
                          AND year = ?
                          AND period_type = ?;
                    )SQL";

                    Stmt st{db_, sql};
                    bind_text(db_, st.get(), 1, ticker);

                    if (sqlite3_bind_int(st.get(), 2, year) != SQLITE_OK)
                        db::detail::throw_sqlite(db_, "bind year failed");

                    bind_text(db_, st.get(), 3, period_type);

                    const int rc = sqlite3_step(st.get());
                    if (rc != SQLITE_DONE)
                        db::detail::throw_sqlite(db_,
                                                 "delete period step failed");
                }
            },
            err);
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}

// *
// **
// ***
// ****
// ***** FINANCES QUERIES

bool Database::add_finances(const std::string& ticker,
                            const std::string& period,
                            const FinancePayload& payload,
                            std::string* err)
{
    try {
        const auto [year, period_type] = parse_period(period);
        const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));

        return in_transaction(
            db_,
            [&] {
                // upsert ticker
                {
                    const char* sql = R"SQL(
                INSERT INTO tickers (ticker, last_update)
                VALUES (?, ?)
                ON CONFLICT(ticker) DO UPDATE SET
                    last_update = excluded.last_update;
            )SQL";

                    Stmt st{db_, sql};
                    bind_text(db_, st.get(), 1, ticker);
                    if (sqlite3_bind_int64(st.get(), 2, now) != SQLITE_OK)
                        db::detail::throw_sqlite(db_, "bind now failed");

                    const int rc = sqlite3_step(st.get());
                    if (rc != SQLITE_DONE)
                        db::detail::throw_sqlite(db_,
                                                 "upsert tickers step failed");
                }

                // upsert finances
                {
                    const char* sql = R"SQL(
                INSERT INTO finances (
                    ticker, year, period_type,
                    current_assets,
                    non_current_assets,
                    eps,
                    cash_and_equivalents,
                    cash_flow_from_financing,
                    cash_flow_from_investing,
                    cash_flow_from_operations,
                    revenue,
                    current_liabilities,
                    non_current_liabilities,
                    net_income
                )
                VALUES (
                    ?, ?, ?,
                    ?, ?, ?,
                    ?, ?, ?, ?,
                    ?, ?, ?, ?
                )
                ON CONFLICT(ticker, year, period_type)
                DO UPDATE SET
                    current_assets            = excluded.current_assets,
                    non_current_assets        = excluded.non_current_assets,
                    eps                       = excluded.eps,
                    cash_and_equivalents      = excluded.cash_and_equivalents,
                    cash_flow_from_financing  = excluded.cash_flow_from_financing,
                    cash_flow_from_investing  = excluded.cash_flow_from_investing,
                    cash_flow_from_operations = excluded.cash_flow_from_operations,
                    revenue                   = excluded.revenue,
                    current_liabilities       = excluded.current_liabilities,
                    non_current_liabilities   = excluded.non_current_liabilities,
                    net_income                = excluded.net_income;
            )SQL";

                    Stmt st{db_, sql};

                    bind_text(db_, st.get(), 1, ticker);
                    if (sqlite3_bind_int(st.get(), 2, year) != SQLITE_OK)
                        db::detail::throw_sqlite(db_, "bind year failed");
                    bind_text(db_, st.get(), 3, period_type);

                    bind_i64_opt(db_, st.get(), 4, payload.current_assets);
                    bind_i64_opt(db_, st.get(), 5, payload.non_current_assets);
                    bind_f64_opt(db_, st.get(), 6, payload.eps);
                    bind_i64_opt(
                        db_, st.get(), 7, payload.cash_and_equivalents);
                    bind_i64_opt(
                        db_, st.get(), 8, payload.cash_flow_from_financing);
                    bind_i64_opt(
                        db_, st.get(), 9, payload.cash_flow_from_investing);
                    bind_i64_opt(
                        db_, st.get(), 10, payload.cash_flow_from_operations);
                    bind_i64_opt(db_, st.get(), 11, payload.revenue);
                    bind_i64_opt(
                        db_, st.get(), 12, payload.current_liabilities);
                    bind_i64_opt(
                        db_, st.get(), 13, payload.non_current_liabilities);
                    bind_i64_opt(db_, st.get(), 14, payload.net_income);

                    const int rc = sqlite3_step(st.get());
                    if (rc != SQLITE_DONE)
                        db::detail::throw_sqlite(db_,
                                                 "upsert finances step failed");
                }
            },
            err);
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}

std::vector<Database::FinanceRow>
Database::get_finances(const std::string& ticker, std::string* err)
{
    try {
        const char* sql = R"SQL(
            SELECT
                ticker, year, period_type,
                current_assets,
                non_current_assets,
                eps,
                cash_and_equivalents,
                cash_flow_from_financing,
                cash_flow_from_investing,
                cash_flow_from_operations,
                revenue,
                current_liabilities,
                non_current_liabilities,
                net_income
            FROM finances
            WHERE ticker = ?
            ORDER BY year ASC, period_type ASC;
        )SQL";

        Stmt st{db_, sql};
        bind_text(db_, st.get(), 1, ticker);

        std::vector<FinanceRow> out;
        while (true) {
            const int rc = sqlite3_step(st.get());
            if (rc == SQLITE_ROW) {
                FinanceRow r;
                r.ticker = col_text(st.get(), 0);
                r.year = sqlite3_column_int(st.get(), 1);
                r.period_type = col_text(st.get(), 2);

                r.current_assets = col_i64_opt(st.get(), 3);
                r.non_current_assets = col_i64_opt(st.get(), 4);
                r.eps = col_f64_opt(st.get(), 5);
                r.cash_and_equivalents = col_i64_opt(st.get(), 6);
                r.cash_flow_from_financing = col_i64_opt(st.get(), 7);
                r.cash_flow_from_investing = col_i64_opt(st.get(), 8);
                r.cash_flow_from_operations = col_i64_opt(st.get(), 9);
                r.revenue = col_i64_opt(st.get(), 10);
                r.current_liabilities = col_i64_opt(st.get(), 11);
                r.non_current_liabilities = col_i64_opt(st.get(), 12);
                r.net_income = col_i64_opt(st.get(), 13);

                out.push_back(std::move(r));
            }
            else if (rc == SQLITE_DONE) {
                break;
            }
            else {
                db::detail::throw_sqlite(db_, "step failed");
            }
        }

        return out;
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return {};
    }
}

} // namespace db

