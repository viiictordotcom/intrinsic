#pragma once
#include <curses.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "state.hpp"

namespace views {

inline constexpr int kHomeGridCols = 3;
inline constexpr int kHomeGridRows = 5;
inline constexpr int kHomeGridGap = 2;
inline constexpr int kHomeMinTextWidth = 8;
inline constexpr int kHomeMaxTextWidth = 18;
inline constexpr int kHomeTextCushion = 2;
inline constexpr int kHomeSearchLimit = 15;
inline constexpr std::size_t kHomeSearchMaxLen = 12;
inline constexpr int kHomePortfolioColorPair = 5;

inline bool prefetch_matches(const AppState& app, int page)
{
    const auto& p = app.tickers.prefetch;
    return p.valid && p.page == page && p.page_size == app.tickers.page_size &&
           p.sort_key == app.settings.sort_key &&
           p.sort_dir == app.settings.sort_dir &&
           p.portfolio_only == app.tickers.portfolio_only;
}

inline std::vector<db::Database::TickerRow>
fetch_page(AppState& app, int page, std::string* err)
{
    if (prefetch_matches(app, page)) {
        auto rows = std::move(app.tickers.prefetch.rows);
        app.tickers.prefetch.valid = false;
        return rows;
    }

    return app.db->get_tickers(page,
                               app.tickers.page_size,
                               app.settings.sort_key,
                               app.settings.sort_dir,
                               err,
                               app.tickers.portfolio_only);
}

inline int home_cell_width(const std::vector<db::Database::TickerRow>& rows)
{
    const std::size_t max_ticker_len =
        std::accumulate(rows.begin(),
                        rows.end(),
                        std::size_t{0},
                        [](std::size_t acc, const auto& row) {
                            return std::max(acc, row.ticker.size());
                        });

    const int text_width =
        std::clamp(static_cast<int>(max_ticker_len) + kHomeTextCushion,
                   kHomeMinTextWidth,
                   kHomeMaxTextWidth);
    return text_width + 2; // marker + space prefix
}

inline int
home_active_grid_cols(int term_cols,
                      const std::vector<db::Database::TickerRow>& rows)
{
    const int col_w = home_cell_width(rows);
    const int required_width =
        (kHomeGridCols * col_w) + (kHomeGridGap * (kHomeGridCols - 1));
    return term_cols >= required_width ? kHomeGridCols : 1;
}

inline int home_active_grid_rows(int count, int grid_cols)
{
    if (grid_cols <= 1) return std::max(1, count);
    return kHomeGridRows;
}

inline int
home_index_for_cell(int count, int col, int row, int grid_cols, int grid_rows)
{
    if (count <= 0) return -1;
    if (col < 0 || col >= grid_cols) return -1;
    if (row < 0 || row >= grid_rows) return -1;
    const int idx = col * grid_rows + row;
    if (idx < 0 || idx >= count) return -1;
    return idx;
}

inline int home_best_index_in_col(
    int count, int col, int preferred_row, int grid_cols, int grid_rows)
{
    if (grid_rows <= 0) return -1;
    preferred_row = std::clamp(preferred_row, 0, grid_rows - 1);

    int idx =
        home_index_for_cell(count, col, preferred_row, grid_cols, grid_rows);
    if (idx >= 0) return idx;

    for (int row = grid_rows - 1; row >= 0; --row) {
        idx = home_index_for_cell(count, col, row, grid_cols, grid_rows);
        if (idx >= 0) return idx;
    }
    return -1;
}

inline bool open_selected_home_ticker(AppState& app)
{
    const auto& rows = app.tickers.last_rows;
    if (rows.empty()) return true;

    if (app.tickers.selected < 0) app.tickers.selected = 0;
    if (app.tickers.selected >= static_cast<int>(rows.size())) {
        app.tickers.selected = static_cast<int>(rows.size() - 1);
    }

    const auto& ticker = rows[app.tickers.selected].ticker;
    std::string err;
    auto finances = app.db->get_finances(ticker, &err);
    if (!err.empty()) {
        route_error(app, err);
        return true;
    }

    app.ticker_view.reset(ticker, std::move(finances), rows[app.tickers.selected].type);
    app.current = views::ViewId::Ticker;
    return true;
}

inline void enter_home_search_mode(AppState& app)
{
    app.tickers.search_mode = true;
    app.tickers.search_exit_armed = false;
    app.tickers.search_query.clear();
    app.tickers.search_submitted_query.clear();
    app.tickers.search_rows.clear();
    app.tickers.selected = 0;
    app.tickers.row_scroll = 0;
}

inline void exit_home_search_mode(AppState& app)
{
    app.tickers.clear_search();
    app.tickers.selected = 0;
    app.tickers.row_scroll = 0;
}

inline bool run_home_search(AppState& app)
{
    std::string err;
    auto rows = app.db->search_tickers(
        app.tickers.search_query,
        kHomeSearchLimit,
        &err,
        app.tickers.portfolio_only);
    if (!err.empty()) {
        route_error(app, err);
        return false;
    }

    app.tickers.search_rows = std::move(rows);
    app.tickers.search_submitted_query = app.tickers.search_query;
    app.tickers.selected = 0;
    app.tickers.row_scroll = 0;
    return true;
}

inline bool toggle_home_portfolio_mode(AppState& app)
{
    app.tickers.portfolio_only = !app.tickers.portfolio_only;
    app.tickers.page = 0;
    app.tickers.invalidate_prefetch();
    app.tickers.selected = 0;
    app.tickers.row_scroll = 0;

    if (!app.tickers.search_mode) return true;

    const bool has_submitted_query = !app.tickers.search_submitted_query.empty();
    const bool showing_submitted_query =
        app.tickers.search_query == app.tickers.search_submitted_query;
    if (has_submitted_query && showing_submitted_query) {
        run_home_search(app);
    }
    else {
        app.tickers.search_rows.clear();
        app.tickers.search_submitted_query.clear();
    }
    return true;
}

inline bool toggle_selected_home_ticker_portfolio(AppState& app)
{
    const auto& rows = app.tickers.last_rows;
    if (rows.empty()) return true;

    if (app.tickers.selected < 0) app.tickers.selected = 0;
    if (app.tickers.selected >= static_cast<int>(rows.size())) {
        app.tickers.selected = static_cast<int>(rows.size() - 1);
    }

    const std::string ticker = rows[app.tickers.selected].ticker;
    std::string err;
    if (!app.db->toggle_ticker_portfolio(ticker, &err)) {
        if (!err.empty()) route_error(app, err);
        return true;
    }

    app.tickers.invalidate_prefetch();
    if (app.tickers.portfolio_only) {
        app.tickers.page = 0;
        app.tickers.selected = 0;
        app.tickers.row_scroll = 0;
    }

    return true;
}

inline bool go_prev_home_page(AppState& app)
{
    if (app.tickers.page <= 0) return true;
    app.tickers.page -= 1;
    app.tickers.invalidate_prefetch();
    app.tickers.selected = 0;
    app.tickers.row_scroll = 0;
    return true;
}

inline bool go_next_home_page(AppState& app)
{
    if (app.tickers.page >= std::numeric_limits<int>::max()) return true;
    const int next_page = app.tickers.page + 1;

    if (prefetch_matches(app, next_page)) {
        if (!app.tickers.prefetch.rows.empty()) {
            app.tickers.page = next_page;
            app.tickers.selected = 0;
            app.tickers.row_scroll = 0;
        }
        return true;
    }

    std::string err;
    auto next_rows = app.db->get_tickers(next_page,
                                         app.tickers.page_size,
                                         app.settings.sort_key,
                                         app.settings.sort_dir,
                                         &err,
                                         app.tickers.portfolio_only);

    if (!err.empty()) {
        route_error(app, err);
        return true;
    }

    if (next_rows.empty()) return true;

    auto& p = app.tickers.prefetch;
    p.page = next_page;
    p.page_size = app.tickers.page_size;
    p.sort_key = app.settings.sort_key;
    p.sort_dir = app.settings.sort_dir;
    p.portfolio_only = app.tickers.portfolio_only;
    p.rows = std::move(next_rows);
    p.valid = true;

    app.tickers.page = next_page;
    app.tickers.selected = 0;
    app.tickers.row_scroll = 0;
    return true;
}

inline void render_home(AppState& app)
{
    if (app.tickers.search_mode)
        curs_set(1);
    else
        curs_set(0);

    erase();

    if (!app.tickers.search_mode) {
        std::string err;
        auto rows = fetch_page(app, app.tickers.page, &err);
        if (!err.empty()) {
            route_error(app, err);
            return;
        }
        app.tickers.last_rows = std::move(rows);
    }
    else {
        app.tickers.last_rows = app.tickers.search_rows;
    }

    const auto& rows_ref = app.tickers.last_rows;

    if (rows_ref.empty()) {
        app.tickers.selected = 0;
        app.tickers.row_scroll = 0;
    }
    else {
        if (app.tickers.selected < 0) app.tickers.selected = 0;
        if (app.tickers.selected >= static_cast<int>(rows_ref.size())) {
            app.tickers.selected = static_cast<int>(rows_ref.size() - 1);
        }
    }

    int grid_y = 2;
    if (app.tickers.search_mode) {
        if (LINES > 0) {
            if (has_colors()) attron(COLOR_PAIR(3));
            attron(A_BOLD);
            mvprintw(0, 0, "intrinsic ~");
            attroff(A_BOLD);
            if (has_colors()) attroff(COLOR_PAIR(3));
            if (COLS > 11) {
                mvprintw(0,
                         11,
                         app.tickers.portfolio_only ? " search portfolio"
                                                    : " search");
            }
        }
        if (LINES > 1) {
            mvprintw(1, 0, "search> %s", app.tickers.search_query.c_str());
        }
        grid_y = 3;

        if (app.tickers.search_submitted_query.empty()) {
            if (LINES > grid_y) mvprintw(grid_y, 0, "type query");
        }
        else if (rows_ref.empty()) {
            if (LINES > grid_y) mvprintw(grid_y, 0, "no matches");
        }

        if (LINES > 1) {
            const int cursor_x =
                8 + static_cast<int>(app.tickers.search_query.size());
            move(1, std::min(cursor_x, std::max(0, COLS - 1)));
        }
    }
    else {
        if (LINES > 0) {
            if (has_colors()) attron(COLOR_PAIR(3));
            attron(A_BOLD);
            mvprintw(0, 0, "intrinsic ~");
            attroff(A_BOLD);
            if (has_colors()) attroff(COLOR_PAIR(3));
            if (COLS > 11) {
                mvprintw(0,
                         11,
                         app.tickers.portfolio_only ? " portfolio page %d"
                                                    : " page %d",
                         app.tickers.page + 1);
            }
        }
    }

    int help_lines = 0;
    if (app.settings.show_help) {
        if (LINES >= 10)
            help_lines = 4;
        else if (LINES >= 8)
            help_lines = 2;
    }

    const int help_start = std::max(0, LINES - help_lines);
    const int grid_cols = home_active_grid_cols(COLS, rows_ref);
    const int grid_rows =
        home_active_grid_rows(static_cast<int>(rows_ref.size()), grid_cols);
    const int grid_rows_visible =
        std::max(0, std::min(grid_rows, help_start - grid_y));
    const int col_w = home_cell_width(rows_ref);
    int row_scroll = app.tickers.row_scroll;

    if (rows_ref.empty() || grid_rows_visible <= 0) {
        row_scroll = 0;
    }
    else {
        const int max_scroll = std::max(0, grid_rows - grid_rows_visible);
        row_scroll = std::clamp(row_scroll, 0, max_scroll);

        const int selected_row = app.tickers.selected % grid_rows;
        if (selected_row < row_scroll) {
            row_scroll = selected_row;
        }
        else if (selected_row >= row_scroll + grid_rows_visible) {
            row_scroll = selected_row - grid_rows_visible + 1;
        }
        row_scroll = std::clamp(row_scroll, 0, max_scroll);
    }

    app.tickers.row_scroll = row_scroll;

    for (std::size_t i = 0; i < rows_ref.size(); ++i) {
        const int idx = static_cast<int>(i);
        const int col = idx / grid_rows;
        const int row = idx % grid_rows;
        if (col >= grid_cols) continue;
        if (row < row_scroll || row >= row_scroll + grid_rows_visible) continue;

        const int y = grid_y + (row - row_scroll);
        const int x = col * (col_w + kHomeGridGap);
        if (x >= COLS) continue;

        const bool selected = idx == app.tickers.selected;
        const bool portfolio = rows_ref[i].portfolio;
        if (selected) attron(A_BOLD);

        const char marker = selected ? '>' : ' ';
        mvaddch(y, x, marker);

        const std::string text = rows_ref[i].ticker;
        const int text_w = std::max(0, std::min(col_w - 2, COLS - (x + 2)));
        const int visible_len =
            std::max(0, std::min(static_cast<int>(text.size()), text_w));
        if (visible_len > 0) {
            if (portfolio && has_colors()) {
                attron(COLOR_PAIR(kHomePortfolioColorPair));
            }
            mvprintw(y, x + 2, "%.*s", visible_len, text.c_str());
            if (portfolio && has_colors()) {
                attroff(COLOR_PAIR(kHomePortfolioColorPair));
            }
        }

        if (selected) attroff(A_BOLD);
    }

    if (rows_ref.empty() && !app.tickers.search_mode) {
        if (LINES > 3) {
            if (app.tickers.portfolio_only) {
                mvprintw(3, 0, "No portfolio tickers. Press 'p' on a ticker.");
            }
            else {
                mvprintw(3, 0, "Press 'a' to start using intrinsic");
            }
        }
    }

    if (help_lines >= 4) {
        const int y0 = std::max(0, LINES - 4);
        if (app.tickers.search_mode) {
            attron(A_DIM);
            mvprintw(y0 + 2, 0, "esc: exit search");
            mvprintw(y0 + 3, 0, "q: quit   s: settings   ?: help");
            attroff(A_DIM);
        }
        else {
            attron(A_DIM);
            mvprintw(y0 + 2, 0, "a: add   p: mark   P: portfolio view   space: search");
            mvprintw(y0 + 3, 0, "q: quit   s: settings   ?: help");
            attroff(A_DIM);
        }
    }
    else if (help_lines >= 2) {
        const int y0 = std::max(0, LINES - 2);
        if (app.tickers.search_mode) {
            attron(A_DIM);
            mvprintw(y0 + 0, 0, "esc: exit search");
            mvprintw(y0 + 1, 0, "q: quit   s: settings   ?: help");
            attroff(A_DIM);
        }
        else {
            attron(A_DIM);
            mvprintw(y0 + 0, 0, "a: add   p: mark   P: portfolio view   space: search");
            mvprintw(y0 + 1, 0, "q: quit   s: settings   ?: help");
            attroff(A_DIM);
        }
    }

    wnoutrefresh(stdscr);
    doupdate();
}

inline bool handle_key_home(AppState& app, int ch)
{
    const int count = static_cast<int>(app.tickers.last_rows.size());
    const int grid_cols = home_active_grid_cols(COLS, app.tickers.last_rows);
    const int grid_rows = home_active_grid_rows(count, grid_cols);

    if (count > 0) {
        app.tickers.selected = std::clamp(app.tickers.selected, 0, count - 1);
    }
    else {
        app.tickers.selected = 0;
    }

    if (ch == 'P') {
        return toggle_home_portfolio_mode(app);
    }

    if (app.tickers.search_mode) {
        if (ch == 27 /*ESC*/) {
            exit_home_search_mode(app);
            return true;
        }

        const int BACKSPACE_1 = KEY_BACKSPACE;
        const int BACKSPACE_2 = 127;
        const int BACKSPACE_3 = 8;

        if (ch == BACKSPACE_1 || ch == BACKSPACE_2 || ch == BACKSPACE_3) {
            if (!app.tickers.search_query.empty()) {
                app.tickers.search_query.pop_back();
            }
            app.tickers.search_submitted_query.clear();
            app.tickers.search_rows.clear();
            return true;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (app.tickers.search_query.empty()) {
                exit_home_search_mode(app);
                return true;
            }

            if (app.tickers.search_submitted_query !=
                app.tickers.search_query) {
                run_home_search(app);
                return true;
            }

            return open_selected_home_ticker(app);
        }

        if (ch >= 0 && ch <= 255) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (std::isalnum(c) || c == '.') {
                const bool start_new =
                    !app.tickers.search_submitted_query.empty() &&
                    app.tickers.search_query ==
                        app.tickers.search_submitted_query;
                if (start_new) {
                    app.tickers.search_query.clear();
                    app.tickers.search_rows.clear();
                    app.tickers.selected = 0;
                }

                if (app.tickers.search_query.size() < kHomeSearchMaxLen) {
                    app.tickers.search_query.push_back(
                        static_cast<char>(std::toupper(c)));
                }
                app.tickers.search_submitted_query.clear();
                return true;
            }
        }
    }
    else {
        if (ch == ' ') {
            enter_home_search_mode(app);
            return true;
        }
        if (ch == 'p') {
            return toggle_selected_home_ticker_portfolio(app);
        }
    }

    if (ch == KEY_UP) {
        if (count <= 0) return true;
        const int col = app.tickers.selected / grid_rows;
        const int row = app.tickers.selected % grid_rows;
        const int target =
            home_index_for_cell(count, col, row - 1, grid_cols, grid_rows);
        if (target >= 0) app.tickers.selected = target;
        return true;
    }

    if (ch == KEY_DOWN) {
        if (count <= 0) return true;
        const int col = app.tickers.selected / grid_rows;
        const int row = app.tickers.selected % grid_rows;
        const int target =
            home_index_for_cell(count, col, row + 1, grid_cols, grid_rows);
        if (target >= 0) app.tickers.selected = target;
        return true;
    }

    if (ch == KEY_LEFT) {
        if (count <= 0) return true;
        const int col = app.tickers.selected / grid_rows;
        const int row = app.tickers.selected % grid_rows;
        const int target =
            home_best_index_in_col(count, col - 1, row, grid_cols, grid_rows);
        if (target >= 0) {
            app.tickers.selected = target;
            return true;
        }
        if (app.tickers.search_mode) return true;
        return go_prev_home_page(app);
    }

    if (ch == KEY_RIGHT) {
        if (count <= 0) return true;
        const int col = app.tickers.selected / grid_rows;
        const int row = app.tickers.selected % grid_rows;
        const int target =
            home_best_index_in_col(count, col + 1, row, grid_cols, grid_rows);
        if (target >= 0) {
            app.tickers.selected = target;
            return true;
        }
        if (app.tickers.search_mode) return true;
        return go_next_home_page(app);
    }

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        return open_selected_home_ticker(app);
    }

    return false; // not handled by home view
}

} // namespace views
