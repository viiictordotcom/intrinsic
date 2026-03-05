#include <curses.h>
#include <csignal>
#include <cstdio>
#include <chrono>
#include <stdexcept>

#include "state.hpp"
#include "settings.hpp"
#include "views/view.hpp"
#include "views/home/view_home.hpp"
#include "views/help/view_help.hpp"
#include "views/settings/view_settings.hpp"
#include "views/error/view_error.hpp"
#include "views/add/view_add.hpp"
#include "views/ticker/view_ticker.hpp"

inline short rgb8_to_ncurses(int channel)
{
    return static_cast<short>((channel * 1000 + 127) / 255);
}

struct AccentColors {
    short green = COLOR_GREEN;
    short red = COLOR_RED;
    short cyan = COLOR_CYAN;
};

inline AccentColors accent_colors_for_mode(ColorMode mode)
{
    AccentColors colors;

    // white and black modes use predefined accents when available
    // default mode keeps terminal-derived colors
    if (mode != ColorMode::White && mode != ColorMode::Black) return colors;
    if (!can_change_color() || COLORS < 16) return colors;

    const short custom_green = static_cast<short>(COLORS - 3);
    const short custom_red = static_cast<short>(COLORS - 2);
    const short custom_cyan = static_cast<short>(COLORS - 1);

    init_color(custom_green,
               rgb8_to_ncurses(0x4F),
               rgb8_to_ncurses(0xAE),
               rgb8_to_ncurses(0x93));
    init_color(custom_red,
               rgb8_to_ncurses(0xCC),
               rgb8_to_ncurses(0x5F),
               rgb8_to_ncurses(0x81));
    init_color(custom_cyan,
               rgb8_to_ncurses(0x3C),
               rgb8_to_ncurses(0x9D),
               rgb8_to_ncurses(0xDA));

    colors.green = custom_green;
    colors.red = custom_red;
    colors.cyan = custom_cyan;
    return colors;
}

inline void configure_theme(const AppState::Settings& settings)
{
    if (!has_colors()) return;

    const short bg = -1;
    const AccentColors accents = accent_colors_for_mode(settings.color_mode);

    init_pair(1, accents.green, bg);
    init_pair(2, accents.red, bg);
    init_pair(3, COLOR_BLUE, bg);
    init_pair(4, accents.cyan, bg);

    if (settings.color_mode == ColorMode::White) {
        init_pair(5, COLOR_BLACK, -1);
        bkgdset(' ' | COLOR_PAIR(5));
        bkgd(' ' | COLOR_PAIR(5));
    }
    else if (settings.color_mode == ColorMode::Black) {
        init_pair(5, COLOR_WHITE, -1);
        bkgdset(' ' | COLOR_PAIR(5));
        bkgd(' ' | COLOR_PAIR(5));
    }
    else {
        bkgdset(' ' | A_NORMAL);
        bkgd(' ' | A_NORMAL);
    }
}

struct Ncurses {
    enum class TerminalBackground {
        Default,
        White,
        Black,
    };

    enum class TerminalCursor {
        Default,
        WhiteDark,
        WhiteBlack,
        BlackLight,
    };

    static void handle_sigint(int) { interrupted_ = 1; }

    static bool interrupt_requested() { return interrupted_ != 0; }

    static bool is_ticker_or_add_view(views::ViewId view_id)
    {
        return view_id == views::ViewId::Ticker ||
               view_id == views::ViewId::Add;
    }

    void sync_terminal_appearance(ColorMode color_mode, views::ViewId view_id)
    {
        TerminalBackground target = TerminalBackground::Default;
        if (color_mode == ColorMode::White) target = TerminalBackground::White;
        if (color_mode == ColorMode::Black) target = TerminalBackground::Black;

        bool changed = false;

        if (target != terminal_background_) {
            if (target == TerminalBackground::White) {
                // OSC 11 sets terminal background color; use pure white.
                std::fputs("\033]11;#ffffff\007", stdout);
            }
            else if (target == TerminalBackground::Black) {
                // OSC 11 sets terminal background color; use pure black.
                std::fputs("\033]11;#000000\007", stdout);
            }
            else {
                // OSC 111 restores terminal default background.
                std::fputs("\033]111\007", stdout);
            }
            terminal_background_ = target;
            changed = true;
        }

        TerminalCursor cursor_target = TerminalCursor::Default;
        if (color_mode == ColorMode::White) {
            cursor_target = is_ticker_or_add_view(view_id)
                                ? TerminalCursor::WhiteBlack
                                : TerminalCursor::WhiteDark;
        }
        else if (color_mode == ColorMode::Black) {
            cursor_target = TerminalCursor::BlackLight;
        }

        if (cursor_target != terminal_cursor_) {
            if (cursor_target == TerminalCursor::WhiteDark) {
                // OSC 12 sets terminal cursor color.
                std::fputs("\033]12;#1f2937\007", stdout);
            }
            else if (cursor_target == TerminalCursor::WhiteBlack) {
                std::fputs("\033]12;#000000\007", stdout);
            }
            else if (cursor_target == TerminalCursor::BlackLight) {
                std::fputs("\033]12;#f5f5f5\007", stdout);
            }
            else {
                // OSC 112 restores terminal default cursor color.
                std::fputs("\033]112\007", stdout);
            }
            terminal_cursor_ = cursor_target;
            changed = true;
        }

        if (changed) std::fflush(stdout);
    }

    Ncurses()
    {
        interrupted_ = 0;
        struct sigaction action{};
        action.sa_handler = &Ncurses::handle_sigint;
        sigemptyset(&action.sa_mask);
        action.sa_flags =
            0; // avoid SA_RESTART so input waits can be interrupted
        has_old_sigint_action_ =
            (sigaction(SIGINT, &action, &old_sigint_action_) == 0);
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
#if defined(NCURSES_VERSION)
        set_escdelay(25);
#endif
        curs_set(0);
        if (has_colors()) {
            start_color();
#if defined(NCURSES_VERSION)
            use_default_colors();
#endif
        }
    }
    ~Ncurses()
    {
        endwin();
        if (terminal_background_ != TerminalBackground::Default ||
            terminal_cursor_ != TerminalCursor::Default) {
            std::fputs("\033]111\007", stdout);
            std::fputs("\033]112\007", stdout);
            std::fflush(stdout);
        }
        if (has_old_sigint_action_) {
            sigaction(SIGINT, &old_sigint_action_, nullptr);
        }
    }

    Ncurses(const Ncurses&) = delete;
    Ncurses& operator=(const Ncurses&) = delete;

private:
    inline static volatile std::sig_atomic_t interrupted_ = 0;
    TerminalBackground terminal_background_ = TerminalBackground::Default;
    TerminalCursor terminal_cursor_ = TerminalCursor::Default;
    struct sigaction old_sigint_action_{};
    bool has_old_sigint_action_ = false;
};

int main()
{
    try {
        db::Database database;
        database.open_or_create();

        Ncurses ncurses;

        AppState app;
        app.db = &database;
        app.current = views::ViewId::Home;

        // load persisted settings
        {
            std::string err;
            if (!load_settings(app.settings, &err)) {
                route_error(app, err);
            }
        }

        while (true) {
            if (Ncurses::interrupt_requested()) break;

            ncurses.sync_terminal_appearance(app.settings.color_mode,
                                             app.current);
            configure_theme(app.settings);

            // render
            switch (app.current) {
            case views::ViewId::Home:
                views::render_home(app);
                break;
            case views::ViewId::Help:
                views::render_help(app);
                break;
            case views::ViewId::Settings:
                views::render_settings(app);
                break;
            case views::ViewId::Ticker:
                views::render_ticker(app);
                break;
            case views::ViewId::Add:
                views::render_add(app);
                break;
            case views::ViewId::Error:
                views::render_error(app);
                break;
            }

            int getch_timeout_ms = -1;
            if (app.current == views::ViewId::Ticker &&
                app.ticker_view.status_line_expires_at.has_value()) {
                const auto now = std::chrono::steady_clock::now();
                const auto expires_at = *app.ticker_view.status_line_expires_at;
                const auto remaining_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        expires_at - now)
                        .count();
                if (remaining_ms <= 0) {
                    getch_timeout_ms = 0;
                }
                else if (remaining_ms < 100) {
                    getch_timeout_ms = static_cast<int>(remaining_ms);
                }
                else {
                    getch_timeout_ms = 100;
                }
            }
            if (getch_timeout_ms < 0) getch_timeout_ms = 50;
            timeout(getch_timeout_ms);
            int ch = getch();
            if (Ncurses::interrupt_requested()) break;
            if (ch == ERR) continue;
            if (ch == 3) break; // Ctrl+C as key event fallback

            // view-local first
            bool consumed = false;

            switch (app.current) {
            case views::ViewId::Home:
                consumed = views::handle_key_home(app, ch);
                break;
            case views::ViewId::Help:
                consumed = views::handle_key_help(app, ch);
                break;
            case views::ViewId::Settings:
                consumed = views::handle_key_settings(app, ch);
                break;
            case views::ViewId::Ticker:
                consumed = views::handle_key_ticker(app, ch);
                break;
            case views::ViewId::Add:
                consumed = views::handle_key_add(app, ch);
                break;
            case views::ViewId::Error:
                consumed = views::handle_key_error(app, ch);
                break;
            }

            if (app.quit_requested) break;
            if (consumed) continue;

            // hard global key
            if (ch == 'q') break;

            // global navigation fallback
            switch (ch) {
            case 'h':
                app.current = views::ViewId::Home;
                break;
            case '?':
                app.current = views::ViewId::Help;
                break;
            case 's':
                app.current = views::ViewId::Settings;
                break;
            case 'a':
                views::open_add_create(app);
                break;
            default:
                break;
            }
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}


