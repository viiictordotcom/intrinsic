#include "settings.hpp"
#include "test_harness.hpp"
#include "test_utils.hpp"

#include <filesystem>
#include <string>

TEST_CASE("settings trim and lower helpers normalize text")
{
    REQUIRE_EQ(trim_copy("  A b  \n"), std::string("A b"));
    REQUIRE_EQ(lower_copy("HeLLo-123"), std::string("hello-123"));
}

TEST_CASE("settings path prefers XDG_CONFIG_HOME over HOME")
{
    test::TempDir temp;
    const auto xdg = temp.path() / "xdg";
    const auto home = temp.path() / "home";

    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", xdg.string());
    test::ScopedEnvVar home_env("HOME", home.string());

    std::string err;
    const auto path = intrinsic_config_path(&err);

    REQUIRE(err.empty());
    REQUIRE_EQ(path, xdg / "intrinsic" / "config.ini");
}

TEST_CASE("settings path uses platform fallback when XDG_CONFIG_HOME is absent")
{
    test::TempDir temp;
    const auto home = temp.path() / "home";

    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", std::nullopt);
    test::ScopedEnvVar home_env("HOME", home.string());

    std::string err;
    const auto path = intrinsic_config_path(&err);

    REQUIRE(err.empty());
#if defined(__APPLE__)
    REQUIRE_EQ(path,
               home / "Library" / "Application Support" / "intrinsic" /
                   "config.ini");
#else
    REQUIRE_EQ(path, home / ".config" / "intrinsic" / "config.ini");
#endif
}

TEST_CASE("settings path returns error when no environment path is available")
{
    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", std::nullopt);
    test::ScopedEnvVar home_env("HOME", std::nullopt);

    std::string err;
    const auto path = intrinsic_config_path(&err);

    REQUIRE(path.empty());
    REQUIRE_CONTAINS(err, "cannot resolve config path");
}

TEST_CASE("settings save and load roundtrip preserves values")
{
    test::TempDir temp;
    const auto xdg = temp.path() / "xdg";

    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", xdg.string());
    test::ScopedEnvVar home_env("HOME", (temp.path() / "home").string());

    AppState::Settings saved{};
    saved.sort_key = db::Database::TickerSortKey::Ticker;
    saved.sort_dir = db::Database::SortDir::Asc;
    saved.ttm = true;
    saved.show_help = false;

    std::string err;
    REQUIRE(save_settings(saved, &err));
    REQUIRE(err.empty());

    AppState::Settings loaded{};
    loaded.sort_key = db::Database::TickerSortKey::LastUpdate;
    loaded.sort_dir = db::Database::SortDir::Desc;
    loaded.ttm = false;
    loaded.show_help = true;

    REQUIRE(load_settings(loaded, &err));
    REQUIRE(err.empty());

    REQUIRE_EQ(loaded.sort_key, saved.sort_key);
    REQUIRE_EQ(loaded.sort_dir, saved.sort_dir);
    REQUIRE_EQ(loaded.ttm, saved.ttm);
    REQUIRE_EQ(loaded.show_help, saved.show_help);
}

TEST_CASE("settings loader handles aliases comments and malformed lines")
{
    namespace fs = std::filesystem;

    test::TempDir temp;
    const auto xdg = temp.path() / "xdg";

    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", xdg.string());
    test::ScopedEnvVar home_env("HOME", (temp.path() / "home").string());

    std::string err;
    const fs::path cfg = intrinsic_config_path(&err);
    REQUIRE(err.empty());

    fs::create_directories(cfg.parent_path());
    test::write_text_file(cfg,
                          "# comment\n"
                          "sort_key = ticker\n"
                          "sort_order = a\n"
                          "ttm = yes\n"
                          "help = off\n"
                          "bad_line_without_equals\n"
                          "sort_key = lastupdate\n");

    AppState::Settings loaded{};
    loaded.sort_key = db::Database::TickerSortKey::LastUpdate;
    loaded.sort_dir = db::Database::SortDir::Desc;
    loaded.ttm = false;
    loaded.show_help = true;

    REQUIRE(load_settings(loaded, &err));
    REQUIRE(err.empty());

    REQUIRE_EQ(loaded.sort_key, db::Database::TickerSortKey::LastUpdate);
    REQUIRE_EQ(loaded.sort_dir, db::Database::SortDir::Asc);
    REQUIRE_EQ(loaded.ttm, true);
    REQUIRE_EQ(loaded.show_help, false);
}

TEST_CASE("settings load succeeds when config file is missing")
{
    test::TempDir temp;
    const auto xdg = temp.path() / "xdg";

    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", xdg.string());
    test::ScopedEnvVar home_env("HOME", (temp.path() / "home").string());

    AppState::Settings loaded{};
    loaded.sort_key = db::Database::TickerSortKey::LastUpdate;
    loaded.sort_dir = db::Database::SortDir::Desc;
    loaded.ttm = false;
    loaded.show_help = true;

    std::string err;
    REQUIRE(load_settings(loaded, &err));
    REQUIRE(err.empty());

    REQUIRE_EQ(loaded.sort_key, db::Database::TickerSortKey::LastUpdate);
    REQUIRE_EQ(loaded.sort_dir, db::Database::SortDir::Desc);
    REQUIRE_EQ(loaded.ttm, false);
    REQUIRE_EQ(loaded.show_help, true);
}

TEST_CASE("settings save fails when config location cannot be resolved")
{
    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", std::nullopt);
    test::ScopedEnvVar home_env("HOME", std::nullopt);

    AppState::Settings settings{};

    std::string err;
    REQUIRE(!save_settings(settings, &err));
    REQUIRE_CONTAINS(err, "cannot resolve config path");
}

TEST_CASE("settings save fails when config directory is not writable")
{
    namespace fs = std::filesystem;

    test::TempDir temp;
    const fs::path xdg = temp.path() / "xdg";
    const fs::path config_dir = xdg / "intrinsic";
    fs::create_directories(config_dir);

    test::ScopedEnvVar xdg_env("XDG_CONFIG_HOME", xdg.string());
    test::ScopedEnvVar home_env("HOME", (temp.path() / "home").string());

    std::error_code perm_err;
    fs::permissions(config_dir,
                    fs::perms::owner_write | fs::perms::group_write |
                        fs::perms::others_write,
                    fs::perm_options::remove,
                    perm_err);
    REQUIRE(!perm_err);

    AppState::Settings settings{};
    std::string err;
    const bool ok = save_settings(settings, &err);

    perm_err.clear();
    fs::permissions(
        config_dir, fs::perms::owner_write, fs::perm_options::add, perm_err);

    REQUIRE(!ok);
    REQUIRE_CONTAINS(err, "failed to open config for writing");
}


