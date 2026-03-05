#pragma once
#include <curses.h>
#include <array>

#include "state.hpp"

namespace views {

inline void render_help(const AppState& app)
{
    curs_set(0);
    erase();
    const bool show_footer_hints = app.settings.show_help;
    if (LINES > 0) {
        if (has_colors()) attron(COLOR_PAIR(3));
        attron(A_BOLD);
        mvprintw(0, 0, "intrinsic ~");
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(3));
        if (COLS > 11) mvprintw(0, 11, " help");
    }

    static constexpr std::array<const char*, 18> lines = {
        "q  - quit",
        "h / esc  - home",
        "?  - help",
        "s  - settings",
        "",
        "a  - add mode",
        "space  - search (home) / switch type (add)",
        "",
        "p  - mark/unmark portfolio ticker",
        "P  - show all or only portfolio tickers",
        "",
        "esc  - exit search/add mode",
        "-  - back to home from ticker",
        "",
        "x  - delete period",
        "e  - edit period",
        "c  - copy period data",
        "y  - toggle yearly/all periods",
    };

    const int start_y = 2;
    const int footer_lines = show_footer_hints ? 1 : 0;
    const int available =
        (LINES > start_y) ? std::max(0, LINES - start_y - footer_lines) : 0;
    const int shown =
        (available > 0) ? std::min<int>(available, lines.size()) : 0;

    for (int i = 0; i < shown; ++i) {
        mvprintw(start_y + i, 2, "%s", lines[static_cast<std::size_t>(i)]);
    }

    if (shown < static_cast<int>(lines.size()) && LINES > 0) {
        const int y = std::max(0, LINES - 1 - footer_lines);
        attron(A_DIM);
        mvprintw(y, 0, "... resize terminal to see all help");
        attroff(A_DIM);
    }

    if (show_footer_hints && LINES > 0) {
        const int max_width = std::max(0, COLS - 1);
        attron(A_DIM);
        mvprintw(
            LINES - 1, 0, "%.*s", max_width, "h / esc: home   ?: help   q: quit");
        attroff(A_DIM);
    }

    wnoutrefresh(stdscr);
    doupdate();
}

inline bool handle_key_help(AppState& app, int ch)
{
    if (ch == 27 /*ESC*/) {
        app.current = views::ViewId::Home;
        return true;
    }
    return false;
}

} // namespace views

