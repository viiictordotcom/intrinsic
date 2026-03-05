#pragma once

#include "db/database.hpp"
#include "state.hpp"
#include "test_utils.hpp"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace test {

inline db::Database::FinancePayload standard_payload(
    std::int64_t revenue = 100, std::int64_t net_income = 10, double eps = 1.0)
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

struct AppSandbox {
    TempDir temp;
    ScopedEnvVar xdg_data;
    ScopedEnvVar xdg_config;
    ScopedEnvVar home;
    db::Database database;
    AppState app;

    AppSandbox()
        : xdg_data("XDG_DATA_HOME", (temp.path() / "xdg-data").string()),
          xdg_config("XDG_CONFIG_HOME", (temp.path() / "xdg-config").string()),
          home("HOME", (temp.path() / "home").string())
    {
        database.open_or_create();
        app.db = &database;
        app.current = views::ViewId::Home;
    }

    std::filesystem::path config_file_path() const
    {
        return temp.path() / "xdg-config" / "intrinsic" / "config.ini";
    }

    void add_finance(
        const std::string& ticker,
        const std::string& period,
        const db::Database::FinancePayload& payload = standard_payload())
    {
        std::string err;
        if (!database.add_finances(ticker, period, payload, &err)) {
            throw std::runtime_error("add_finance failed: " + err);
        }
    }
};

} // namespace test


