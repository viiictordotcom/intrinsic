#pragma once
#include <curses.h>
#include <array>

#include "state.hpp"

namespace views {

inline void render_help(AppState&)
{
    curs_set(0);
    erase();
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
        "h  - home",
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
    const int available = (LINES > start_y) ? (LINES - start_y) : 0;
    const int shown =
        (available > 0) ? std::min<int>(available, lines.size()) : 0;

    for (int i = 0; i < shown; ++i) {
        mvprintw(start_y + i, 2, "%s", lines[static_cast<std::size_t>(i)]);
    }

    if (shown < static_cast<int>(lines.size()) && LINES > 1) {
        attron(A_DIM);
        mvprintw(LINES - 1, 0, "... resize terminal to see all help");
        attroff(A_DIM);
    }

    wnoutrefresh(stdscr);
    doupdate();
}

inline bool handle_key_help(AppState&, int)
{
    return false;
}

} // namespace views
