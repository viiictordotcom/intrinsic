#pragma once
#include <curses.h>
#include <algorithm>
#include <string>

#include "state.hpp"

namespace views {

inline void render_error(const AppState& app)
{
    curs_set(0);
    erase();
    if (LINES > 0) {
        if (has_colors()) attron(COLOR_PAIR(3));
        attron(A_BOLD);
        mvprintw(0, 0, "intrinsic ~");
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(3));
        if (COLS > 11) mvprintw(0, 11, " error");
    }

    const int start_y = 2;
    const int footer_lines = 2;
    const int message_limit = std::max(0, LINES - start_y - footer_lines);
    const int width = std::max(8, COLS - 1);

    int y = start_y;
    for (std::size_t i = 0;
         i < app.last_error.size() && y < start_y + message_limit;) {
        const std::size_t remaining = app.last_error.size() - i;
        const int chunk =
            std::min<std::size_t>(remaining, static_cast<std::size_t>(width));
        mvprintw(y++, 0, "%.*s", chunk, app.last_error.c_str() + i);
        i += static_cast<std::size_t>(chunk);
    }

    attron(A_DIM);
    if (LINES > 1) mvprintw(LINES - 2, 0, "h: home   q: quit");
    if (LINES > 0) mvprintw(LINES - 1, 0, "?: help   s: settings");
    attroff(A_DIM);

    wnoutrefresh(stdscr);
    doupdate();
}

inline bool handle_key_error(AppState&, int)
{
    return false;
}

} // namespace views


