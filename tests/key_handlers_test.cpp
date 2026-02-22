#include "settings.hpp"
#include "test_fixture.hpp"
#include "test_harness.hpp"
#include "views/view_add.hpp"
#include "views/view_home.hpp"
#include "views/view_settings.hpp"
#include "views/view_ticker.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

TEST_CASE("key_home search flow transitions into ticker view")
{
    test::AppSandbox sandbox;
    sandbox.add_finance("AAPL", "2024-Y");
    sandbox.add_finance("MSFT", "2024-Y");

    std::string err;
    sandbox.app.tickers.last_rows =
        sandbox.database.get_tickers(0,
                                     20,
                                     db::Database::TickerSortKey::Ticker,
                                     db::Database::SortDir::Asc,
                                     &err);
    REQUIRE(err.empty());

    REQUIRE(views::handle_key_home(sandbox.app, ' '));
    REQUIRE(sandbox.app.tickers.search_mode);

    for (char c : std::string("aapl")) {
        REQUIRE(views::handle_key_home(sandbox.app, c));
    }

    REQUIRE_EQ(sandbox.app.tickers.search_query, std::string("AAPL"));

    REQUIRE(views::handle_key_home(sandbox.app, '\n'));
    REQUIRE_EQ(sandbox.app.tickers.search_submitted_query, std::string("AAPL"));
    REQUIRE_EQ(sandbox.app.tickers.search_rows.size(), std::size_t{1});

    sandbox.app.tickers.last_rows = sandbox.app.tickers.search_rows;
    REQUIRE(views::handle_key_home(sandbox.app, '\n'));
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Ticker);
    REQUIRE_EQ(sandbox.app.ticker_view.ticker, std::string("AAPL"));
}

TEST_CASE("key_home enforces search length limit and exits search mode")
{
    test::AppSandbox sandbox;

    REQUIRE(views::handle_key_home(sandbox.app, ' '));
    REQUIRE(sandbox.app.tickers.search_mode);

    for (int i = 0; i < 64; ++i) {
        REQUIRE(views::handle_key_home(sandbox.app, 'x'));
    }
    REQUIRE_EQ(sandbox.app.tickers.search_query.size(),
               views::kHomeSearchMaxLen);

    const std::string before = sandbox.app.tickers.search_query;
    REQUIRE(!views::handle_key_home(sandbox.app, 1));
    REQUIRE_EQ(sandbox.app.tickers.search_query, before);

    while (!sandbox.app.tickers.search_query.empty()) {
        REQUIRE(views::handle_key_home(sandbox.app, 127));
    }
    REQUIRE(views::handle_key_home(sandbox.app, '\n'));
    REQUIRE(!sandbox.app.tickers.search_mode);
}

TEST_CASE("key_home p toggles selected ticker portfolio membership")
{
    test::AppSandbox sandbox;
    sandbox.add_finance("AAPL", "2024-Y");

    std::string err;
    sandbox.app.tickers.last_rows =
        sandbox.database.get_tickers(0,
                                     20,
                                     db::Database::TickerSortKey::Ticker,
                                     db::Database::SortDir::Asc,
                                     &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(sandbox.app.tickers.last_rows.size(), std::size_t{1});

    REQUIRE(views::handle_key_home(sandbox.app, 'p'));

    const auto portfolio_only =
        sandbox.database.get_tickers(0,
                                     20,
                                     db::Database::TickerSortKey::Ticker,
                                     db::Database::SortDir::Asc,
                                     &err,
                                     true);
    REQUIRE(err.empty());
    REQUIRE_EQ(portfolio_only.size(), std::size_t{1});
    REQUIRE_EQ(portfolio_only.front().ticker, std::string("AAPL"));

    REQUIRE(views::handle_key_home(sandbox.app, 'p'));

    const auto none = sandbox.database.get_tickers(0,
                                                   20,
                                                   db::Database::TickerSortKey::Ticker,
                                                   db::Database::SortDir::Asc,
                                                   &err,
                                                   true);
    REQUIRE(err.empty());
    REQUIRE(none.empty());
}

TEST_CASE("key_home P toggles portfolio mode and scopes search")
{
    test::AppSandbox sandbox;
    sandbox.add_finance("AAPL", "2024-Y");
    sandbox.add_finance("MSFT", "2024-Y");

    std::string err;
    REQUIRE(sandbox.database.toggle_ticker_portfolio("AAPL", &err));
    REQUIRE(err.empty());

    sandbox.app.tickers.page = 4;
    sandbox.app.tickers.selected = 3;
    sandbox.app.tickers.prefetch.valid = true;

    REQUIRE(views::handle_key_home(sandbox.app, 'P'));
    REQUIRE(sandbox.app.tickers.portfolio_only);
    REQUIRE_EQ(sandbox.app.tickers.page, 0);
    REQUIRE_EQ(sandbox.app.tickers.selected, 0);
    REQUIRE(!sandbox.app.tickers.prefetch.valid);

    const auto rows = views::fetch_page(sandbox.app, 0, &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(rows.size(), std::size_t{1});
    REQUIRE_EQ(rows.front().ticker, std::string("AAPL"));

    REQUIRE(views::handle_key_home(sandbox.app, ' '));
    REQUIRE(sandbox.app.tickers.search_mode);
    for (char c : std::string("msft")) {
        REQUIRE(views::handle_key_home(sandbox.app, c));
    }
    REQUIRE(views::handle_key_home(sandbox.app, '\n'));
    REQUIRE_EQ(sandbox.app.tickers.search_submitted_query, std::string("MSFT"));
    REQUIRE(sandbox.app.tickers.search_rows.empty());

    REQUIRE(views::handle_key_home(sandbox.app, 'P'));
    REQUIRE(!sandbox.app.tickers.portfolio_only);
}

TEST_CASE("key_add create flow stores finance record and returns home")
{
    test::AppSandbox sandbox;

    views::open_add_create(sandbox.app);
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Add);

    for (char c : std::string("msft")) {
        REQUIRE(views::handle_key_add(sandbox.app, c));
    }
    REQUIRE_EQ(sandbox.app.add.buffers[0], std::string("MSFT"));

    REQUIRE(views::handle_key_add(sandbox.app, '@'));
    REQUIRE_EQ(sandbox.app.add.buffers[0], std::string("MSFT"));

    REQUIRE(views::handle_key_add(sandbox.app, KEY_DOWN));
    REQUIRE_EQ(sandbox.app.add.index, 1);

    for (char c : std::string("2024-y")) {
        REQUIRE(views::handle_key_add(sandbox.app, c));
    }
    REQUIRE_EQ(sandbox.app.add.buffers[1], std::string("2024-Y"));

    REQUIRE(views::handle_key_add(sandbox.app, '\n'));
    REQUIRE(sandbox.app.add.confirming);

    REQUIRE(views::handle_key_add(sandbox.app, 'y'));
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Home);
    REQUIRE(!sandbox.app.add.active);

    std::string err;
    const auto rows = sandbox.database.get_finances("MSFT", &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(rows.size(), std::size_t{1});
    REQUIRE_EQ(rows.front().period_type, std::string("Y"));
}

TEST_CASE("key_add enforces numeric digit limits and edit escape routing")
{
    test::AppSandbox sandbox;

    views::open_add_create(sandbox.app);
    sandbox.app.add.index = 7; // revenue field
    sandbox.app.add.cursor = 0;

    REQUIRE(views::handle_key_add(sandbox.app, '-'));
    REQUIRE(sandbox.app.add.buffers[7].empty());

    for (int i = 0; i < 32; ++i) {
        REQUIRE(views::handle_key_add(sandbox.app, '9'));
    }
    REQUIRE_EQ(sandbox.app.add.buffers[7].size(), std::size_t{15});

    sandbox.app.add.index = 8; // net income allows negatives
    sandbox.app.add.cursor = 0;
    REQUIRE(views::handle_key_add(sandbox.app, '-'));
    REQUIRE_EQ(sandbox.app.add.buffers[8], std::string("-"));

    sandbox.add_finance("IBM", "2024-Y");
    std::string err;
    const auto rows = sandbox.database.get_finances("IBM", &err);
    REQUIRE(err.empty());
    REQUIRE_EQ(rows.size(), std::size_t{1});

    views::open_add_prefilled_from_ticker(sandbox.app, rows.front());
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Add);
    REQUIRE(views::handle_key_add(sandbox.app, 27));
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Ticker);
}

TEST_CASE("key_ticker navigation input bounds and yearly toggle work")
{
    test::AppSandbox sandbox;
    sandbox.add_finance("IBM", "2023-Y");
    sandbox.add_finance("IBM", "2024-Q1");
    sandbox.add_finance("IBM", "2024-Y");

    std::string err;
    auto rows = sandbox.database.get_finances("IBM", &err);
    REQUIRE(err.empty());

    sandbox.app.ticker_view.reset("IBM", rows);
    sandbox.app.current = views::ViewId::Ticker;

    const int start_index = sandbox.app.ticker_view.index;
    REQUIRE(views::handle_key_ticker(sandbox.app, KEY_LEFT));
    REQUIRE_EQ(sandbox.app.ticker_view.index, start_index - 1);
    REQUIRE(views::handle_key_ticker(sandbox.app, KEY_RIGHT));
    REQUIRE_EQ(sandbox.app.ticker_view.index, start_index);

    sandbox.app.ticker_view.inputs[0] = "123";
    REQUIRE(views::handle_key_ticker(sandbox.app, 127));
    REQUIRE_EQ(sandbox.app.ticker_view.inputs[0], std::string("12"));

    REQUIRE(views::handle_key_ticker(sandbox.app, KEY_DC));
    REQUIRE(sandbox.app.ticker_view.inputs[0].empty());

    for (std::size_t i = 0; i < views::kTickerInputMaxLen; ++i) {
        REQUIRE(views::handle_key_ticker(sandbox.app, '1'));
    }
    REQUIRE(!views::handle_key_ticker(sandbox.app, '1'));
    REQUIRE_EQ(sandbox.app.ticker_view.inputs[0].size(),
               views::kTickerInputMaxLen);

    REQUIRE(views::handle_key_ticker(sandbox.app, 'y'));
    REQUIRE(sandbox.app.ticker_view.yearly_only);
    REQUIRE_EQ(sandbox.app.ticker_view.rows.size(), std::size_t{2});

    REQUIRE(views::handle_key_ticker(sandbox.app, 'y'));
    REQUIRE(!sandbox.app.ticker_view.yearly_only);
    REQUIRE_EQ(sandbox.app.ticker_view.rows.size(), std::size_t{3});

    REQUIRE(views::handle_key_ticker(sandbox.app, 'e'));
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Add);
}

TEST_CASE("key_ticker delete on last period returns to home")
{
    test::AppSandbox sandbox;
    sandbox.add_finance("ONE", "2024-Y");

    std::string err;
    auto rows = sandbox.database.get_finances("ONE", &err);
    REQUIRE(err.empty());

    sandbox.app.ticker_view.reset("ONE", rows);
    sandbox.app.current = views::ViewId::Ticker;

    REQUIRE(views::handle_key_ticker(sandbox.app, 'x'));
    REQUIRE_EQ(sandbox.app.current, views::ViewId::Home);
}

TEST_CASE("key_settings toggles values persists settings and arms nuke")
{
    test::AppSandbox sandbox;

    sandbox.app.tickers.page = 7;
    sandbox.app.tickers.prefetch.valid = true;

    const auto old_sort_key = sandbox.app.settings.sort_key;
    const auto old_sort_dir = sandbox.app.settings.sort_dir;
    const bool old_ttm = sandbox.app.settings.ttm;
    const bool old_help = sandbox.app.settings.show_help;

    REQUIRE(views::handle_key_settings(sandbox.app, 'S'));
    REQUIRE(sandbox.app.settings.sort_key != old_sort_key);
    REQUIRE_EQ(sandbox.app.tickers.page, 0);
    REQUIRE(!sandbox.app.tickers.prefetch.valid);

    REQUIRE(views::handle_key_settings(sandbox.app, 'O'));
    REQUIRE(sandbox.app.settings.sort_dir != old_sort_dir);

    REQUIRE(views::handle_key_settings(sandbox.app, 'T'));
    REQUIRE(sandbox.app.settings.ttm != old_ttm);

    REQUIRE(views::handle_key_settings(sandbox.app, 'H'));
    REQUIRE(sandbox.app.settings.show_help != old_help);

    REQUIRE(std::filesystem::exists(sandbox.config_file_path()));

    REQUIRE(views::handle_key_settings(sandbox.app, 'N'));
    REQUIRE(sandbox.app.settings_view.nuke_confirm_armed);

    REQUIRE(!views::handle_key_settings(sandbox.app, 'z'));
    REQUIRE(!sandbox.app.settings_view.nuke_confirm_armed);
    REQUIRE(!sandbox.app.settings_view.update_confirm_armed);
    REQUIRE(sandbox.app.current != views::ViewId::Error);
}

TEST_CASE("key_settings update binding confirms and runs update command")
{
    test::AppSandbox sandbox;
    test::ScopedEnvVar update_cmd("INTRINSIC_UPDATE_CMD", std::string("true"));

    REQUIRE(views::handle_key_settings(sandbox.app, 'U'));
    REQUIRE(sandbox.app.settings_view.update_confirm_armed);
    REQUIRE_CONTAINS(sandbox.app.settings_view.update_status_line,
                     "press U again");

    REQUIRE(views::handle_key_settings(sandbox.app, 'U'));
    REQUIRE(!sandbox.app.settings_view.update_confirm_armed);
    REQUIRE_CONTAINS(sandbox.app.settings_view.update_status_line,
                     "restart intrinsic");
}

TEST_CASE("key_settings update binding reports failures from update command")
{
    test::AppSandbox sandbox;
    test::ScopedEnvVar update_cmd("INTRINSIC_UPDATE_CMD", std::string("false"));

    REQUIRE(views::handle_key_settings(sandbox.app, 'U'));
    REQUIRE(sandbox.app.settings_view.update_confirm_armed);

    REQUIRE(views::handle_key_settings(sandbox.app, 'U'));
    REQUIRE(!sandbox.app.settings_view.update_confirm_armed);
    REQUIRE_CONTAINS(sandbox.app.settings_view.update_status_line,
                     "update failed");
}

