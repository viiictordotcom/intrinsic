#include "db/database.hpp"
#include "test_harness.hpp"
#include "test_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

db::Database::FinancePayload make_payload(std::int64_t revenue = 100,
                                          std::int64_t net_income = 10,
                                          double eps = 1.0)
{
    db::Database::FinancePayload payload{};
    payload.current_assets = 1000;
    payload.non_current_assets = 5000;
    payload.eps = eps;
    payload.cash_and_equivalents = 300;
    payload.cash_flow_from_financing = -20;
    payload.cash_flow_from_investing = -40;
    payload.cash_flow_from_operations = 70;
    payload.revenue = revenue;
    payload.current_liabilities = 800;
    payload.non_current_liabilities = 2000;
    payload.net_income = net_income;
    return payload;
}

void open_test_db(db::Database& database, const std::filesystem::path& root)
{
    test::ScopedEnvVar xdg_data("XDG_DATA_HOME", root.string());
    test::ScopedEnvVar home("HOME", (root / "home").string());
    database.open_or_create();
}

void create_legacy_schema_db(const std::filesystem::path& db_path)
{
    std::error_code ec;
    std::filesystem::create_directories(db_path.parent_path(), ec);
    if (ec) {
        throw std::runtime_error("failed to create db directory");
    }

    sqlite3* raw = nullptr;
    const int open_rc = sqlite3_open_v2(db_path.string().c_str(),
                                        &raw,
                                        SQLITE_OPEN_READWRITE |
                                            SQLITE_OPEN_CREATE |
                                            SQLITE_OPEN_FULLMUTEX,
                                        nullptr);
    if (open_rc != SQLITE_OK || !raw) {
        if (raw) sqlite3_close(raw);
        throw std::runtime_error("failed to create legacy sqlite file");
    }

    auto exec = [raw](const char* sql) {
        char* err = nullptr;
        const int rc = sqlite3_exec(raw, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string msg = err ? err : "sqlite exec error";
            sqlite3_free(err);
            sqlite3_close(raw);
            throw std::runtime_error(msg);
        }
    };

    exec("PRAGMA foreign_keys = ON;");
    exec(R"SQL(
        CREATE TABLE tickers (
            ticker TEXT PRIMARY KEY,
            last_update INTEGER NOT NULL
        ) WITHOUT ROWID;
    )SQL");
    exec(
        "INSERT INTO tickers (ticker, last_update) VALUES ('LEGACY', 123456);");

    sqlite3_close(raw);
}

std::vector<std::string>
to_tickers(const std::vector<db::Database::TickerRow>& rows)
{
    std::vector<std::string> tickers;
    tickers.reserve(rows.size());
    std::transform(rows.begin(),
                   rows.end(),
                   std::back_inserter(tickers),
                   [](const auto& row) { return row.ticker; });
    return tickers;
}

} // namespace

TEST_CASE("database open_or_create creates file under XDG_DATA_HOME")
{
    test::TempDir temp;

    db::Database database;
    open_test_db(database, temp.path());

    const auto expected = temp.path() / "intrinsic" / "intrinsic.db";
    REQUIRE_EQ(database.path(), expected);
    REQUIRE(std::filesystem::exists(expected));
}

TEST_CASE("database open_or_create migrates legacy ticker schema once")
{
    test::TempDir temp;
    test::ScopedEnvVar xdg_data("XDG_DATA_HOME", temp.path().string());
    test::ScopedEnvVar home("HOME", (temp.path() / "home").string());

    const std::filesystem::path db_path =
        temp.path() / "intrinsic" / "intrinsic.db";
    create_legacy_schema_db(db_path);

    db::Database database;
    database.open_or_create();

    std::string err;
    const auto all = database.get_tickers(0,
                                          10,
                                          db::Database::TickerSortKey::Ticker,
                                          db::Database::SortDir::Asc,
                                          &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(all.size(), std::size_t{1});
    REQUIRE_EQ(all.front().ticker, std::string("LEGACY"));
    REQUIRE(!all.front().portfolio);
    REQUIRE_EQ(all.front().type, 1);

    REQUIRE(database.toggle_ticker_portfolio("LEGACY", &err));
    REQUIRE(err.empty());

    const auto portfolio_rows =
        database.get_tickers(0,
                             10,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Asc,
                             &err,
                             true);
    REQUIRE(err.empty());
    REQUIRE_EQ(portfolio_rows.size(), std::size_t{1});
    REQUIRE(portfolio_rows.front().portfolio);

    database.close();
    database.open_or_create();

    const auto persisted =
        database.get_tickers(0,
                             10,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Asc,
                             &err,
                             true);
    REQUIRE(err.empty());
    REQUIRE_EQ(persisted.size(), std::size_t{1});
    REQUIRE_EQ(persisted.front().ticker, std::string("LEGACY"));
    REQUIRE(persisted.front().portfolio);
    REQUIRE_EQ(persisted.front().type, 1);
}

TEST_CASE(
    "database open_or_create throws when HOME and XDG_DATA_HOME are absent")
{
    test::ScopedEnvVar xdg_data("XDG_DATA_HOME", std::nullopt);
    test::ScopedEnvVar home("HOME", std::nullopt);

    db::Database database;
    REQUIRE_THROWS(database.open_or_create());
}

TEST_CASE("database open_or_create fails when data directory is not writable")
{
    namespace fs = std::filesystem;

    test::TempDir temp;
    const fs::path blocked_root = temp.path() / "blocked-data";
    fs::create_directories(blocked_root);

    test::ScopedEnvVar xdg_data("XDG_DATA_HOME", blocked_root.string());
    test::ScopedEnvVar home("HOME", (temp.path() / "home").string());

    std::error_code perm_err;
    fs::permissions(blocked_root,
                    fs::perms::owner_write | fs::perms::group_write |
                        fs::perms::others_write,
                    fs::perm_options::remove,
                    perm_err);
    REQUIRE(!perm_err);

    db::Database database;
    bool threw = false;
    try {
        database.open_or_create();
    }
    catch (...) {
        threw = true;
    }

    perm_err.clear();
    fs::permissions(
        blocked_root, fs::perms::owner_write, fs::perm_options::add, perm_err);

    REQUIRE(threw);
}

TEST_CASE("database add_finances and get_finances roundtrip nullable fields")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    db::Database::FinancePayload payload{};
    payload.current_assets = 120;
    payload.non_current_assets = std::nullopt;
    payload.eps = 1.25;
    payload.cash_and_equivalents = std::nullopt;
    payload.cash_flow_from_financing = -9;
    payload.cash_flow_from_investing = -7;
    payload.cash_flow_from_operations = 42;
    payload.revenue = 200;
    payload.current_liabilities = 33;
    payload.non_current_liabilities = std::nullopt;
    payload.net_income = 15;

    std::string err;
    REQUIRE(database.add_finances("AAPL", "2024-Y", payload, &err));
    REQUIRE(err.empty());

    const auto finances = database.get_finances("AAPL", &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(finances.size(), std::size_t{1});

    const auto& row = finances.front();
    REQUIRE_EQ(row.ticker, std::string("AAPL"));
    REQUIRE_EQ(row.year, 2024);
    REQUIRE_EQ(row.period_type, std::string("Y"));
    REQUIRE_EQ(row.current_assets, payload.current_assets);
    REQUIRE_EQ(row.non_current_assets, payload.non_current_assets);
    REQUIRE_EQ(row.eps, payload.eps);
    REQUIRE_EQ(row.cash_and_equivalents, payload.cash_and_equivalents);
    REQUIRE_EQ(row.non_current_liabilities, payload.non_current_liabilities);

    const auto tickers =
        database.get_tickers(0,
                             10,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Asc,
                             &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(tickers.size(), std::size_t{1});
    REQUIRE_EQ(tickers.front().ticker, std::string("AAPL"));
    REQUIRE(tickers.front().last_update > 0);
    REQUIRE_EQ(tickers.front().type, 1);
}

TEST_CASE("database stores ticker type and rejects mixed type periods")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    db::Database::FinancePayload bank{};
    bank.total_loans = 1000;
    bank.goodwill = 50;
    bank.total_assets = 5000;
    bank.total_deposits = 4000;
    bank.total_liabilities = 4500;
    bank.net_interest_income = 120;
    bank.non_interest_income = 30;
    bank.loan_loss_provisions = 10;
    bank.non_interest_expense = 90;
    bank.net_income = 40;
    bank.eps = 2.0;
    bank.risk_weighted_assets = 3000;
    bank.common_equity_tier1 = 360;
    bank.net_charge_offs = 8;
    bank.non_performing_loans = 20;

    std::string err;
    REQUIRE(database.add_finances("BANK", "2024-Y", bank, &err, 2));
    REQUIRE(err.empty());

    const auto tickers =
        database.get_tickers(0,
                             10,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Asc,
                             &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(tickers.size(), std::size_t{1});
    REQUIRE_EQ(tickers.front().type, 2);

    const auto rows = database.get_finances("BANK", &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(rows.size(), std::size_t{1});
    REQUIRE_EQ(rows.front().total_loans, bank.total_loans);
    REQUIRE_EQ(rows.front().common_equity_tier1, bank.common_equity_tier1);

    err.clear();
    REQUIRE(!database.add_finances("BANK", "2025-Y", make_payload(), &err, 1));
    REQUIRE_CONTAINS(err, "ticker type mismatch");
}

TEST_CASE("database add_finances upserts existing period")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string err;
    REQUIRE(database.add_finances(
        "MSFT", "2023-Y", make_payload(100, 10, 1.0), &err));
    REQUIRE(database.add_finances(
        "MSFT", "2023-Y", make_payload(999, 99, 9.9), &err));

    const auto finances = database.get_finances("MSFT", &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(finances.size(), std::size_t{1});
    REQUIRE_EQ(finances.front().revenue, std::optional<std::int64_t>{999});
    REQUIRE_EQ(finances.front().net_income, std::optional<std::int64_t>{99});
    REQUIRE_EQ(finances.front().eps, std::optional<double>{9.9});
}

TEST_CASE("database portfolio toggles and filters get/search ticker queries")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string err;
    REQUIRE(database.add_finances("AAPL", "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances("MSFT", "2024-Y", make_payload(), &err));

    const auto all = database.get_tickers(0,
                                          10,
                                          db::Database::TickerSortKey::Ticker,
                                          db::Database::SortDir::Asc,
                                          &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(all.size(), std::size_t{2});
    REQUIRE(!all[0].portfolio);
    REQUIRE(!all[1].portfolio);

    REQUIRE(database.toggle_ticker_portfolio("AAPL", &err));
    REQUIRE(err.empty());

    const auto portfolio_only =
        database.get_tickers(0,
                             10,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Asc,
                             &err,
                             true);
    REQUIRE(err.empty());
    REQUIRE_EQ(portfolio_only.size(), std::size_t{1});
    REQUIRE_EQ(portfolio_only.front().ticker, std::string("AAPL"));
    REQUIRE(portfolio_only.front().portfolio);

    const auto search_portfolio = database.search_tickers("AAP", 10, &err, true);
    REQUIRE(err.empty());
    REQUIRE_EQ(search_portfolio.size(), std::size_t{1});
    REQUIRE_EQ(search_portfolio.front().ticker, std::string("AAPL"));
    REQUIRE(search_portfolio.front().portfolio);

    const auto search_filtered_out =
        database.search_tickers("MSF", 10, &err, true);
    REQUIRE(err.empty());
    REQUIRE(search_filtered_out.empty());

    REQUIRE(database.toggle_ticker_portfolio("AAPL", &err));
    REQUIRE(err.empty());

    const auto none = database.get_tickers(0,
                                           10,
                                           db::Database::TickerSortKey::Ticker,
                                           db::Database::SortDir::Asc,
                                           &err,
                                           true);
    REQUIRE(err.empty());
    REQUIRE(none.empty());
}

TEST_CASE("database get_tickers supports deterministic sort and pagination")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string err;
    REQUIRE(database.add_finances("MSFT", "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances("AAPL", "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances("GOOG", "2024-Y", make_payload(), &err));

    const auto asc_rows =
        database.get_tickers(0,
                             10,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Asc,
                             &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(to_tickers(asc_rows),
               std::vector<std::string>({"AAPL", "GOOG", "MSFT"}));

    const auto desc_rows =
        database.get_tickers(0,
                             10,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Desc,
                             &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(to_tickers(desc_rows),
               std::vector<std::string>({"MSFT", "GOOG", "AAPL"}));

    const auto page0 = database.get_tickers(0,
                                            2,
                                            db::Database::TickerSortKey::Ticker,
                                            db::Database::SortDir::Asc,
                                            &err);
    const auto page1 = database.get_tickers(1,
                                            2,
                                            db::Database::TickerSortKey::Ticker,
                                            db::Database::SortDir::Asc,
                                            &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(to_tickers(page0), std::vector<std::string>({"AAPL", "GOOG"}));
    REQUIRE_EQ(to_tickers(page1), std::vector<std::string>({"MSFT"}));

    const auto normalized =
        database.get_tickers(-3,
                             0,
                             db::Database::TickerSortKey::Ticker,
                             db::Database::SortDir::Asc,
                             &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(normalized.size(), std::size_t{1});
    REQUIRE_EQ(normalized.front().ticker, std::string("AAPL"));
}

TEST_CASE("database search_tickers is case insensitive and obeys limit")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string err;
    REQUIRE(database.add_finances("AAPL", "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances("AAL", "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances("MSFT", "2024-Y", make_payload(), &err));

    const auto one = database.search_tickers("aa", 1, &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(to_tickers(one), std::vector<std::string>({"AAL"}));

    const auto two = database.search_tickers("AA", 10, &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(to_tickers(two), std::vector<std::string>({"AAL", "AAPL"}));

    const auto empty = database.search_tickers("", 10, &err);
    REQUIRE(err.empty());
    REQUIRE(empty.empty());
}

TEST_CASE("database delete_period removes one row then cascades last row")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string err;
    REQUIRE(database.add_finances("IBM", "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances("IBM", "2025-Y", make_payload(), &err));

    REQUIRE(database.delete_period("IBM", "2024-Y", &err));
    REQUIRE(err.empty());

    auto remaining = database.get_finances("IBM", &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(remaining.size(), std::size_t{1});
    REQUIRE_EQ(remaining.front().year, 2025);

    auto tickers = database.search_tickers("IBM", 5, &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(tickers.size(), std::size_t{1});

    REQUIRE(database.delete_period("IBM", "2025-Y", &err));
    REQUIRE(err.empty());

    remaining = database.get_finances("IBM", &err);
    REQUIRE(err.empty());
    REQUIRE(remaining.empty());

    tickers = database.search_tickers("IBM", 5, &err);
    REQUIRE(err.empty());
    REQUIRE(tickers.empty());

    REQUIRE(!database.delete_period("IBM", "2025-Y", &err));
    REQUIRE(!err.empty());
}

TEST_CASE("database reports invalid period input")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string err;
    REQUIRE(!database.add_finances("AAPL", "2024", make_payload(), &err));
    REQUIRE_CONTAINS(err, "invalid period format");

    err.clear();
    REQUIRE(!database.delete_period("AAPL", "bad", &err));
    REQUIRE_CONTAINS(err, "invalid period format");
}

TEST_CASE("database get_tickers guards against page offset overflow")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string err;
    const auto rows = database.get_tickers(std::numeric_limits<int>::max(),
                                           std::numeric_limits<int>::max(),
                                           db::Database::TickerSortKey::Ticker,
                                           db::Database::SortDir::Asc,
                                           &err);

    REQUIRE(rows.empty());
    REQUIRE_CONTAINS(err, "page offset out of range");
}

TEST_CASE("database query bindings resist SQL injection strings")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    const std::string injected = "AAPL'; DROP TABLE tickers;--";

    std::string err;
    REQUIRE(database.add_finances(injected, "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances("SAFE", "2024-Y", make_payload(), &err));

    const auto matches = database.search_tickers("DROP TABLE", 10, &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(matches.size(), std::size_t{1});
    REQUIRE_EQ(matches.front().ticker, injected);

    REQUIRE(database.add_finances("NEXT", "2024-Y", make_payload(), &err));
    const auto all = database.get_tickers(0,
                                          10,
                                          db::Database::TickerSortKey::Ticker,
                                          db::Database::SortDir::Asc,
                                          &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(to_tickers(all),
               std::vector<std::string>({injected, "NEXT", "SAFE"}));
}

TEST_CASE("database stores int64 boundary values safely")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    db::Database::FinancePayload payload{};
    payload.current_assets = std::numeric_limits<std::int64_t>::max();
    payload.non_current_assets = std::numeric_limits<std::int64_t>::min();
    payload.eps = 0.0;
    payload.cash_and_equivalents = std::numeric_limits<std::int64_t>::max();
    payload.cash_flow_from_financing = std::numeric_limits<std::int64_t>::min();
    payload.cash_flow_from_investing = std::numeric_limits<std::int64_t>::max();
    payload.cash_flow_from_operations =
        std::numeric_limits<std::int64_t>::min();
    payload.revenue = std::numeric_limits<std::int64_t>::max();
    payload.current_liabilities = std::numeric_limits<std::int64_t>::min();
    payload.non_current_liabilities = std::numeric_limits<std::int64_t>::max();
    payload.net_income = std::numeric_limits<std::int64_t>::min();

    std::string err;
    REQUIRE(database.add_finances("BOUND", "2024-Y", payload, &err));
    REQUIRE(err.empty());

    const auto finances = database.get_finances("BOUND", &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(finances.size(), std::size_t{1});

    const auto& row = finances.front();
    REQUIRE_EQ(row.current_assets, payload.current_assets);
    REQUIRE_EQ(row.non_current_assets, payload.non_current_assets);
    REQUIRE_EQ(row.cash_flow_from_financing, payload.cash_flow_from_financing);
    REQUIRE_EQ(row.revenue, payload.revenue);
    REQUIRE_EQ(row.net_income, payload.net_income);
}

TEST_CASE("database handles long and non-utf8 ticker inputs safely")
{
    test::TempDir temp;
    db::Database database;
    open_test_db(database, temp.path());

    std::string long_ticker(4096, 'A');
    long_ticker[0] = 'L';

    std::string odd_ticker = "ODD";
    odd_ticker.push_back(static_cast<char>(0xFF));
    odd_ticker.push_back(static_cast<char>(0xFE));
    odd_ticker += "X";

    std::string err;
    REQUIRE(database.add_finances(long_ticker, "2024-Y", make_payload(), &err));
    REQUIRE(database.add_finances(odd_ticker, "2024-Y", make_payload(), &err));
    REQUIRE(err.empty());

    const auto long_rows = database.get_finances(long_ticker, &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(long_rows.size(), std::size_t{1});
    REQUIRE_EQ(long_rows.front().ticker, long_ticker);

    const auto odd_rows = database.get_finances(odd_ticker, &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(odd_rows.size(), std::size_t{1});
    REQUIRE_EQ(odd_rows.front().ticker, odd_ticker);
}

TEST_CASE("database supports concurrent writes across separate connections")
{
    test::TempDir temp;
    test::ScopedEnvVar xdg_data("XDG_DATA_HOME", temp.path().string());
    test::ScopedEnvVar home("HOME", (temp.path() / "home").string());

    db::Database left;
    db::Database right;
    left.open_or_create();
    right.open_or_create();

    std::mutex errors_mu;
    std::vector<std::string> errors;

    auto writer = [&](db::Database& database, const std::string& prefix) {
        for (int i = 0; i < 50; ++i) {
            std::string err;
            const std::string ticker = prefix + std::to_string(i);
            const bool ok = database.add_finances(
                ticker, "2024-Y", make_payload(100 + i, 10 + i, 1.0), &err);
            if (!ok || !err.empty()) {
                std::lock_guard<std::mutex> lock(errors_mu);
                errors.push_back(prefix + ":" + err);
            }
        }
    };

    std::thread t1(writer, std::ref(left), "L");
    std::thread t2(writer, std::ref(right), "R");
    t1.join();
    t2.join();

    REQUIRE(errors.empty());

    db::Database verifier;
    verifier.open_or_create();
    std::string err;
    const auto rows = verifier.get_tickers(0,
                                           200,
                                           db::Database::TickerSortKey::Ticker,
                                           db::Database::SortDir::Asc,
                                           &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(rows.size(), std::size_t{100});
}
