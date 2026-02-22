#pragma once
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <curses.h>
#include <cstring>
#include <exception>
#include <filesystem>
#include <sys/wait.h>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#include "state.hpp"
#include "settings.hpp"

namespace views {

inline const char* sort_key_label(db::Database::TickerSortKey k)
{
    return (k == db::Database::TickerSortKey::LastUpdate) ? "last_update"
                                                          : "ticker";
}

inline const char* sort_dir_label(db::Database::SortDir d)
{
    return (d == db::Database::SortDir::Desc) ? "desc" : "asc";
}

inline void print_centered_line(int y, const char* text)
{
    if (!text || y < 0 || y >= LINES || COLS <= 0) return;

    const int max_w = std::max(0, COLS - 1);
    const int len = static_cast<int>(std::strlen(text));
    const int shown = std::min(len, max_w);
    const int x = std::max(0, (COLS - shown) / 2);

    mvprintw(y, x, "%.*s", shown, text);
}

inline const std::array<const char*, 5>& nuke_countdown_art(int value)
{
    static constexpr std::array<const char*, 5> art5 = {
        " #######   ",
        " ##        ",
        " #######   ",
        "      ##   ",
        " #######   ",
    };
    static constexpr std::array<const char*, 5> art4 = {
        " ##   ##   ",
        " ##   ##   ",
        " #######   ",
        "      ##   ",
        "      ##   ",
    };
    static constexpr std::array<const char*, 5> art3 = {
        " #######   ",
        "      ##   ",
        "   #####   ",
        "      ##   ",
        " #######   ",
    };
    static constexpr std::array<const char*, 5> art2 = {
        " #######   ",
        "      ##   ",
        " #######   ",
        " ##        ",
        " #######   ",
    };
    static constexpr std::array<const char*, 5> art1 = {
        "   ###     ",
        "  ####     ",
        "   ###     ",
        "   ###     ",
        " #######   ",
    };
    static constexpr std::array<const char*, 5> art0 = {
        " #######   ",
        " ##   ##   ",
        " ##   ##   ",
        " ##   ##   ",
        " #######   ",
    };

    if (value == 5) return art5;
    if (value == 4) return art4;
    if (value == 3) return art3;
    if (value == 2) return art2;
    if (value == 1) return art1;
    return art0;
}

inline void render_nuke_countdown_frame(int value, bool sparkle)
{
    erase();
    curs_set(0);

    if (LINES > 0) {
        print_centered_line(
            0, sparkle ? "*  *  *  *  *  *  *  *" : ".  .  .  .  .  .  .  .");
    }

    if (LINES > 1) {
        if (has_colors()) attron(COLOR_PAIR(2));
        attron(A_BOLD);
        print_centered_line(1, "!! INTRINSIC NUCLEAR RESET !!");
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(2));
    }

    if (LINES > 3) {
        print_centered_line(3, "all data and settings will be vaporized");
    }

    const auto& art = nuke_countdown_art(value);
    const int art_h = static_cast<int>(art.size());
    const int start_y = std::max(5, (LINES - art_h) / 2);

    for (int i = 0; i < art_h; ++i) {
        if (has_colors()) attron(COLOR_PAIR(1));
        attron(A_BOLD);
        print_centered_line(start_y + i, art[static_cast<std::size_t>(i)]);
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(1));
    }

    if (LINES > start_y + art_h + 1) {
        print_centered_line(start_y + art_h + 1, "brace for clean slate");
    }
    if (LINES > 0) {
        print_centered_line(LINES - 1,
                            sparkle ? "*  *  *  *  *  *  *  *"
                                    : ".  .  .  .  .  .  .  .");
    }

    wnoutrefresh(stdscr);
    doupdate();
}

inline void run_nuke_countdown_easter_egg()
{
    for (int value = 5; value >= 1; --value) {
        for (int phase = 0; phase < 3; ++phase) {
            render_nuke_countdown_frame(value, (phase % 2) == 0);
            napms(150);
        }
        render_nuke_countdown_frame(value, true);
        napms(400);
    }

    render_nuke_countdown_frame(0, true);
    napms(300);
}

inline bool remove_tree_if_exists(const std::filesystem::path& path,
                                  const char* label,
                                  std::string* err)
{
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        if (err) {
            *err =
                std::string("failed checking ") + label + ": " + ec.message();
        }
        return false;
    }

    if (!exists) return true;

    ec.clear();
    std::filesystem::remove_all(path, ec);
    if (ec) {
        if (err) {
            *err =
                std::string("failed removing ") + label + ": " + ec.message();
        }
        return false;
    }

    return true;
}

inline void render_settings(const AppState& app)
{
    curs_set(0);
    erase();

    if (LINES > 0) {
        if (has_colors()) attron(COLOR_PAIR(3));
        attron(A_BOLD);
        mvprintw(0, 0, "intrinsic ~");
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(3));
        if (COLS > 11) mvprintw(0, 11, " settings");
    }
    if (LINES > 2) {
        attron(A_DIM);
        mvprintw(2, 0, "use keys (H/S/O/T/U/N):");
        attroff(A_DIM);
    }

    int y = 4;
    if (LINES > y)
        mvprintw(
            y, 2, "H  show_help : %s", app.settings.show_help ? "on" : "off");
    y += 1;
    if (LINES > y)
        mvprintw(
            y, 2, "S  sort_key  : %s", sort_key_label(app.settings.sort_key));
    y += 1;
    if (LINES > y)
        mvprintw(
            y, 2, "O  sort_dir  : %s", sort_dir_label(app.settings.sort_dir));
    y += 1;
    if (LINES > y)
        mvprintw(y, 2, "T  TTM       : %s", app.settings.ttm ? "on" : "off");
    y += 2;
    if (LINES > y) {
        if (app.settings_view.update_confirm_armed) {
            mvprintw(y, 2, "U  update    : run updater now (press U again)");
        }
        else {
            mvprintw(y, 2, "U  update    : check/apply nix profile update");
        }
    }
    y += 2;
    if (LINES > y) {
        if (app.settings_view.nuke_confirm_armed) {
            mvprintw(
                y, 2, "N  nuke      : initiate final sequence (press N again)");
        }
        else {
            mvprintw(y, 2, "N  nuke      : initiate self-destruct (all data)");
        }
    }

    y += 2;
    if (!app.settings_view.update_status_line.empty() && LINES > y) {
        attron(A_DIM);
        mvprintw(y, 2, "%s", app.settings_view.update_status_line.c_str());
        attroff(A_DIM);
    }

    if (LINES > 1) {
        attron(A_DIM);
        mvprintw(LINES - 1,
                 0,
                 "h: home   ?: help   q: quit   update may require restart");
        attroff(A_DIM);
    }

    wnoutrefresh(stdscr);
    doupdate();
}

inline void apply_settings_changed(AppState& app)
{
    app.tickers.page = 0;
    app.tickers.invalidate_prefetch();

    std::string err;
    if (!save_settings(app.settings, &err)) {
        route_error(app, err);
    }
}

inline bool command_exists_on_path(const char* command)
{
    if (!command || !*command) return false;

    // If an explicit path is provided, test it directly.
    if (std::strchr(command, '/')) return ::access(command, X_OK) == 0;

    const char* raw_path = std::getenv("PATH");
    if (!raw_path || !*raw_path) return false;

    const std::string path_value(raw_path);
    std::size_t start = 0;
    while (start <= path_value.size()) {
        const std::size_t end = path_value.find(':', start);
        const std::string entry = (end == std::string::npos)
                                      ? path_value.substr(start)
                                      : path_value.substr(start, end - start);
        const std::filesystem::path candidate =
            entry.empty() ? std::filesystem::path(command)
                          : std::filesystem::path(entry) / command;
        if (::access(candidate.c_str(), X_OK) == 0) return true;

        if (end == std::string::npos) break;
        start = end + 1;
    }

    return false;
}

#if defined(INTRINSIC_TESTING)
inline bool update_test_override_enabled(bool* success)
{
    if (!success) return false;
    const char* override_cmd = std::getenv("INTRINSIC_UPDATE_CMD");
    if (!override_cmd || !*override_cmd) return false;

    const std::string value(override_cmd);
    if (value == "true" || value == "1" || value == "success") {
        *success = true;
        return true;
    }
    if (value == "false" || value == "0" || value == "fail" ||
        value == "failure") {
        *success = false;
        return true;
    }
    return false;
}
#endif

inline bool update_supported(std::string* reason)
{
#if defined(INTRINSIC_TESTING)
    bool unused = false;
    if (update_test_override_enabled(&unused)) return true;
#endif

    if (command_exists_on_path("nix")) return true;

    if (reason) *reason = "updates unavailable (nix not found)";
    return false;
}

inline bool run_update_command(std::string* err)
{
#if defined(INTRINSIC_TESTING)
    bool test_success = false;
    if (update_test_override_enabled(&test_success)) {
        if (test_success) return true;
        if (err) {
            *err = "update failed or package not installed via nix profile";
        }
        return false;
    }
#endif

    def_prog_mode();
    endwin();

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("nix"));
    argv.push_back(const_cast<char*>("profile"));
    argv.push_back(const_cast<char*>("upgrade"));
    argv.push_back(const_cast<char*>("intrinsic"));
    argv.push_back(const_cast<char*>("--refresh"));
    argv.push_back(nullptr);

    int rc = 1;
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::execvp(argv[0], argv.data());
        std::_Exit(127);
    }
    else if (pid < 0) {
        rc = 1;
    }
    else {
        int status = 0;
        while (::waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR) continue;
            status = 1;
            break;
        }

        if (WIFEXITED(status)) {
            rc = WEXITSTATUS(status);
        }
    }

    reset_prog_mode();
    refresh();

    if (rc == 0) return true;

    if (err) {
        *err = "update failed or package not installed via nix profile";
    }
    return false;
}

inline void nuke_and_reset_app(AppState& app)
{
    db::Database* db = app.db;
    auto reopen_after_failure = [db] {
        if (!db) return;
        try {
            db->open_or_create();
        }
        catch (...) {
        }
    };

    try {
        if (!db) {
            route_error(app, "database not initialized");
            return;
        }

        namespace fs = std::filesystem;
        const fs::path data_dir = db->path().parent_path();

        std::string cfg_err;
        const fs::path cfg = intrinsic_config_path(&cfg_err);
        if (cfg.empty()) {
            route_error(app, cfg_err);
            return;
        }
        const fs::path config_dir = cfg.parent_path();

        db->close();

        std::string remove_err;
        if (!data_dir.empty() &&
            !remove_tree_if_exists(data_dir, "data directory", &remove_err)) {
            reopen_after_failure();
            route_error(app, remove_err);
            return;
        }
        if (!config_dir.empty() && !remove_tree_if_exists(config_dir,
                                                          "config directory",
                                                          &remove_err)) {
            reopen_after_failure();
            route_error(app, remove_err);
            return;
        }

        db->open_or_create();

        AppState fresh;
        fresh.db = db;
        app = std::move(fresh);
    }
    catch (const std::exception& e) {
        reopen_after_failure();
        route_error(app, e.what());
    }
}

inline bool handle_key_settings(AppState& app, int ch)
{
    if (ch != 'N') app.settings_view.nuke_confirm_armed = false;
    if (ch != 'U') app.settings_view.update_confirm_armed = false;

    if (ch == 'S') {
        app.settings.sort_key =
            (app.settings.sort_key == db::Database::TickerSortKey::LastUpdate)
                ? db::Database::TickerSortKey::Ticker
                : db::Database::TickerSortKey::LastUpdate;
        apply_settings_changed(app);
        return true;
    }

    if (ch == 'O') {
        app.settings.sort_dir =
            (app.settings.sort_dir == db::Database::SortDir::Desc)
                ? db::Database::SortDir::Asc
                : db::Database::SortDir::Desc;
        apply_settings_changed(app);
        return true;
    }

    if (ch == 'T') {
        app.settings.ttm = !app.settings.ttm;
        apply_settings_changed(app);
        return true;
    }

    if (ch == 'H') {
        app.settings.show_help = !app.settings.show_help;
        apply_settings_changed(app);
        return true;
    }

    if (ch == 'U') {
        std::string support_reason;
        if (!update_supported(&support_reason)) {
            app.settings_view.update_status_line = std::move(support_reason);
            return true;
        }

        if (!app.settings_view.update_confirm_armed) {
            app.settings_view.update_confirm_armed = true;
            app.settings_view.update_status_line =
                "press U again to run update command";
            return true;
        }

        app.settings_view.update_confirm_armed = false;
        std::string update_err;
        if (run_update_command(&update_err)) {
            app.settings_view.update_status_line =
                "update complete, restart intrinsic to use newest build";
            app.quit_requested = true;
        }
        else {
            app.settings_view.update_status_line = std::move(update_err);
        }
        return true;
    }

    if (ch == 'N') {
        if (!app.settings_view.nuke_confirm_armed) {
            app.settings_view.nuke_confirm_armed = true;
            return true;
        }

        app.settings_view.nuke_confirm_armed = false;
        run_nuke_countdown_easter_egg();
        nuke_and_reset_app(app);
        return true;
    }

    return false;
}

} // namespace views
