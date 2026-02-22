#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace db {

class Database {
public:
    // *
    // **
    // ***
    // ****
    // ***** MODELS

    struct TickerRow {
        std::string ticker;
        std::int64_t last_update = 0;
        bool portfolio = false;
    };

    enum class TickerSortKey { Ticker, LastUpdate };
    enum class SortDir { Asc, Desc };

    struct FinanceRow {
        std::string ticker;
        int year;
        std::string period_type;

        std::optional<std::int64_t> current_assets;
        std::optional<std::int64_t> non_current_assets;
        std::optional<double> eps;
        std::optional<std::int64_t> cash_and_equivalents;
        std::optional<std::int64_t> cash_flow_from_financing;
        std::optional<std::int64_t> cash_flow_from_investing;
        std::optional<std::int64_t> cash_flow_from_operations;
        std::optional<std::int64_t> revenue;
        std::optional<std::int64_t> current_liabilities;
        std::optional<std::int64_t> non_current_liabilities;
        std::optional<std::int64_t> net_income;
    };

    struct FinancePayload {
        std::optional<std::int64_t> current_assets;
        std::optional<std::int64_t> non_current_assets;
        std::optional<double> eps;
        std::optional<std::int64_t> cash_and_equivalents;
        std::optional<std::int64_t> cash_flow_from_financing;
        std::optional<std::int64_t> cash_flow_from_investing;
        std::optional<std::int64_t> cash_flow_from_operations;
        std::optional<std::int64_t> revenue;
        std::optional<std::int64_t> current_liabilities;
        std::optional<std::int64_t> non_current_liabilities;
        std::optional<std::int64_t> net_income;
    };

public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void close();
    void open_or_create();

    const std::filesystem::path& path() const { return db_path_; }

    // *
    // **
    // ***
    // ****
    // ***** QUERIES

    std::vector<TickerRow> get_tickers(int page,
                                       int page_size,
                                       TickerSortKey key,
                                       SortDir dir,
                                       std::string* err = nullptr,
                                       bool portfolio_only = false);

    std::vector<TickerRow> search_tickers(const std::string& contains,
                                          int limit,
                                          std::string* err = nullptr,
                                          bool portfolio_only = false);

    bool toggle_ticker_portfolio(const std::string& ticker,
                                 std::string* err = nullptr);

    bool delete_period(const std::string& ticker,
                       const std::string& period,
                       std::string* err);

    bool add_finances(const std::string& ticker,
                      const std::string& period,
                      const FinancePayload& payload,
                      std::string* err = nullptr);

    std::vector<FinanceRow> get_finances(const std::string& ticker,
                                         std::string* err = nullptr);

private:
    static std::filesystem::path default_db_path_();
    static void
    ensure_parent_dir_exists_(const std::filesystem::path& file_path);

    void open_connection_(const std::filesystem::path& file_path);
    void apply_schema_();

private:
    sqlite3* db_{nullptr};
    std::filesystem::path db_path_{};
};

} // namespace db

