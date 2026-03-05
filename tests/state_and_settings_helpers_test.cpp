#include "state.hpp"
#include "test_harness.hpp"
#include "test_utils.hpp"
#include "views/view_settings.hpp"

#include <filesystem>
#include <string>

TEST_CASE("state route_error updates message and switches to error view")
{
    AppState app;
    app.current = views::ViewId::Home;

    route_error(app, "boom");
    REQUIRE_EQ(app.current, views::ViewId::Error);
    REQUIRE_EQ(app.last_error, std::string("boom"));

    route_error(app, "");
    REQUIRE_EQ(app.last_error, std::string("Unknown error"));

    route_error(app, static_cast<const char*>(nullptr));
    REQUIRE_EQ(app.last_error, std::string("Unknown error"));
}

TEST_CASE("state add and ticker helpers clamp and reset safely")
{
    AddState add;
    add.reset(3);
    REQUIRE(add.active);
    REQUIRE_EQ(add.buffers.size(), std::size_t{3});
    REQUIRE_EQ(add.values.size(), std::size_t{3});
    REQUIRE_EQ(add.layout_y.size(), std::size_t{3});

    AppState::TickerViewState view;
    view.rows.clear();
    view.index = 9;
    view.clamp_index();
    REQUIRE_EQ(view.index, 0);

    db::Database::FinanceRow row{};
    row.ticker = "AAPL";
    view.rows.push_back(row);
    view.index = -5;
    view.clamp_index();
    REQUIRE_EQ(view.index, 0);
}

TEST_CASE(
    "view_settings remove_tree_if_exists handles existing and missing paths")
{
    namespace fs = std::filesystem;

    test::TempDir temp;
    const fs::path tree = temp.path() / "to-remove";
    fs::create_directories(tree / "nested");
    test::write_text_file(tree / "nested" / "data.txt", "x");

    std::string err;
    REQUIRE(views::remove_tree_if_exists(tree, "test tree", &err));
    REQUIRE(err.empty());
    REQUIRE(!fs::exists(tree));

    REQUIRE(views::remove_tree_if_exists(tree, "test tree", &err));
    REQUIRE(err.empty());
}

TEST_CASE("view_settings remove_tree_if_exists does not follow symlink targets")
{
    namespace fs = std::filesystem;

    test::TempDir temp;
    const fs::path target = temp.path() / "target-tree";
    const fs::path link = temp.path() / "linked-tree";
    fs::create_directories(target / "nested");
    test::write_text_file(target / "nested" / "keep.txt", "safe");

    std::error_code symlink_err;
    fs::create_directory_symlink(target, link, symlink_err);
    if (symlink_err) return;

    std::string err;
    REQUIRE(views::remove_tree_if_exists(link, "linked tree", &err));
    REQUIRE(err.empty());
    REQUIRE(!fs::exists(link));
    REQUIRE(fs::exists(target / "nested" / "keep.txt"));
}


