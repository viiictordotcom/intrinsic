#include <curses.h>
#include <cstdio>
#include <stdexcept>

#include "state.hpp"
#include "settings.hpp"
#include "views/view.hpp"
#include "views/view_home.hpp"
#include "views/view_help.hpp"
#include "views/view_settings.hpp"
#include "views/view_error.hpp"
#include "views/view_add.hpp"
#include "views/view_ticker.hpp"

struct Ncurses {
    Ncurses()
    {
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
            init_pair(1, COLOR_GREEN, -1);
            init_pair(2, COLOR_RED, -1);
            init_pair(3, COLOR_BLUE, -1);
            init_pair(4, COLOR_CYAN, -1);
            const short bright_cyan = (COLORS > 14) ? 14 : COLOR_CYAN;
            init_pair(5, COLOR_BLACK, bright_cyan);
        }
    }
    ~Ncurses() { endwin(); }

    Ncurses(const Ncurses&) = delete;
    Ncurses& operator=(const Ncurses&) = delete;
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

            int ch = getch();

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
