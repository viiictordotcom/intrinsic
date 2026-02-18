#include "settings.hpp"
#include "test_fixture.hpp"
#include "test_harness.hpp"
#include "views/view_settings.hpp"

#include <filesystem>
#include <string>

TEST_CASE("nuke_and_reset_app wipes data and config then reinitializes app")
{
    test::AppSandbox sandbox;
    sandbox.add_finance("AAPL", "2024-Y");
    sandbox.add_finance("AAPL", "2024-Q1");

    sandbox.app.current = views::ViewId::Ticker;
    sandbox.app.last_error = "stale";
    sandbox.app.tickers.page = 99;
    sandbox.app.settings.sort_key = db::Database::TickerSortKey::Ticker;
    sandbox.app.settings.sort_dir = db::Database::SortDir::Asc;
    sandbox.app.settings.ttm = true;
    sandbox.app.settings.show_help = false;

    std::string err;
    REQUIRE(save_settings(sandbox.app.settings, &err));
    REQUIRE(err.empty());
    REQUIRE(std::filesystem::exists(sandbox.config_file_path()));
    REQUIRE(std::filesystem::exists(sandbox.database.path()));

    views::nuke_and_reset_app(sandbox.app);

    REQUIRE_EQ(sandbox.app.db, &sandbox.database);
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Home);
    REQUIRE(sandbox.app.last_error.empty());
    REQUIRE_EQ(sandbox.app.tickers.page, 0);

    REQUIRE(std::filesystem::exists(sandbox.database.path()));
    REQUIRE(!std::filesystem::exists(sandbox.config_file_path()));

    const auto rows =
        sandbox.database.get_tickers(0,
                                     10,
                                     db::Database::TickerSortKey::Ticker,
                                     db::Database::SortDir::Asc,
                                     &err);
    REQUIRE(err.empty());
    REQUIRE(rows.empty());
}

TEST_CASE("nuke_and_reset_app reports error when database is missing")
{
    AppState app;
    app.current = views::ViewId::Home;

    views::nuke_and_reset_app(app);

    REQUIRE_EQ(app.current, views::ViewId::Error);
    REQUIRE_CONTAINS(app.last_error, "database not initialized");
}


