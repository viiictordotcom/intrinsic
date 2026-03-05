#pragma once

#include "views/add/view_add_form.hpp"

namespace views {
inline void render_add(AppState& app);

inline void render_blinking_add_input_caret(const AppState& app, int y, int x)
{
    curs_set(0);
    if (y < 0 || y >= LINES || x < 0 || x >= COLS) return;
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
    if (((now_ms / 500) % 2) != 0) return;
    const bool use_mode_text_pair =
        has_colors() && (app.settings.color_mode == ColorMode::White ||
                         app.settings.color_mode == ColorMode::Black);
    if (use_mode_text_pair) attron(COLOR_PAIR(5));
    mvaddch(y, x, '|');
    if (use_mode_text_pair) attroff(COLOR_PAIR(5));
}

inline void flash_add_invalid_marker(AppState& app, int index, int column)
{
    render_add(app);
    const int y =
        (index >= 0 && index < static_cast<int>(app.add.layout_y.size()))
            ? app.add.layout_y[static_cast<std::size_t>(index)]
            : -1;
    if (y >= 0 && y < LINES) {
        const int input_x = add_input_x_for_column(add_input_x(app), column);
        mvaddch(y, input_x, 'x' | A_BOLD);
        wnoutrefresh(stdscr);
        doupdate();
        napms(1500);
        flushinp();
    }
}

inline void render_add(AppState& app)
{
    ensure_add_initialized(app);
    clamp_add_cursor(app);

    erase();

    const int input_x = add_input_x_for_column(add_input_x(app), 0);
    const int input_x_extra = add_input_x_for_column(add_input_x(app), 1);
    const bool two_column_allowed = app.add.mode == AddMode::Create;
    const bool too_narrow_for_two_column =
        add_terminal_too_narrow_for_two_column_mode(COLS, input_x);
    const bool two_column_mode_active = two_column_allowed &&
                                        app.add.value_columns > 1 &&
                                        !too_narrow_for_two_column;
    const auto& fields = add_fields_for_type(app.add.ticker_type);
    const auto& sections = add_sections_for_type(app.add.ticker_type);
    clamp_add_index(app, static_cast<int>(fields.size()));

    int y = 0;
    y += 1; // title
    y += 1; // spacer

    std::vector<int> logical_field_y(fields.size(), 0);
    for (int i = 0; i < (int)fields.size(); ++i) {
        const auto section_it =
            std::find_if(sections.begin(), sections.end(), [&](const auto& s) {
                return s.field_index == i;
            });
        if (section_it != sections.end()) y += 2; // spacer + section title
        logical_field_y[i] = y;
        y += 1;
    }

    y += 1; // spacer before confirm
    const int confirm_y = y;
    if (app.add.confirming) y += 1;
    const int total_lines = y;

    const int viewport =
        (total_lines > LINES) ? std::max(1, LINES - 1) : std::max(1, LINES);
    const int focus_y =
        app.add.confirming ? confirm_y : logical_field_y[app.add.index];
    const int max_scroll = std::max(0, total_lines - viewport);

    app.add.scroll = std::clamp(app.add.scroll, 0, max_scroll);
    if (focus_y < app.add.scroll) app.add.scroll = focus_y;
    if (focus_y >= app.add.scroll + viewport) {
        app.add.scroll = focus_y - viewport + 1;
    }
    app.add.scroll = std::clamp(app.add.scroll, 0, max_scroll);

    auto to_screen_y = [&](int logical_y) {
        return logical_y - app.add.scroll;
    };

    int logical_y = 0;
    int screen_y = to_screen_y(logical_y);
    if (screen_y >= 0 && screen_y < viewport) {
        std::string suffix =
            (app.add.mode == AddMode::EditFromTicker) ? " edit" : " add";
        if (app.add.ticker_type == kAddTickerType1A) {
            suffix += " t1 (a)";
        }
        else if (app.add.ticker_type == kAddTickerType1B) {
            suffix += " t1 (b)";
        }
        else if (app.add.ticker_type == kAddTickerType2) {
            suffix += " t2 (bank)";
        }
        else if (app.add.ticker_type == kAddTickerType3) {
            suffix += " t3 (insurer)";
        }
        if (app.add.mode == AddMode::Create && !app.add.ticker_type_locked) {
            suffix += "";
        }
        else if (app.add.ticker_type_locked) {
            suffix += " [lock]";
        }
        if (has_colors()) attron(COLOR_PAIR(3));
        attron(A_BOLD);
        mvprintw(screen_y, 0, "intrinsic ~");
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(3));
        if (COLS > 11) mvprintw(screen_y, 11, "%s", suffix.c_str());
    }
    logical_y += 2;

    const int label_w = std::max(1, input_x - kAddInputTab - kAddInputCushion);

    for (int i = 0; i < (int)fields.size(); ++i) {
        const auto section_it =
            std::find_if(sections.begin(), sections.end(), [&](const auto& s) {
                return s.field_index == i;
            });
        if (section_it != sections.end()) {
            logical_y += 1;
            screen_y = to_screen_y(logical_y);
            if (screen_y >= 0 && screen_y < viewport) {
                mvprintw(screen_y, 0, "%s", section_it->title);
            }
            logical_y += 1;
        }

        screen_y = to_screen_y(logical_y);
        app.add.layout_y[i] =
            (screen_y >= 0 && screen_y < viewport) ? screen_y : -1;

        if (screen_y >= 0 && screen_y < viewport) {
            const bool is_current = (!app.add.confirming && i == app.add.index);
            const auto& shown = app.add.buffers[static_cast<std::size_t>(i)];

            mvaddch(screen_y, 0, is_current ? ('>' | A_BOLD) : ' ');
            mvprintw(screen_y, kAddInputTab, "%-*s", label_w, fields[i].label);
            mvprintw(screen_y, input_x, "%s", shown.c_str());

            if (two_column_mode_active &&
                fields[static_cast<std::size_t>(i)].key != FieldKey::Ticker &&
                i < static_cast<int>(app.add.buffers_extra.size())) {
                if (input_x_extra - 2 >= 0 && input_x_extra - 2 < COLS) {
                    mvaddch(screen_y, input_x_extra - 2, '|');
                }
                const auto& shown_extra =
                    app.add.buffers_extra[static_cast<std::size_t>(i)];
                mvprintw(screen_y, input_x_extra, "%s", shown_extra.c_str());
            }
        }
        logical_y += 1;
    }

    logical_y += 1;
    int desired_cursor_y = -1;
    int desired_cursor_x = 0;
    if (app.add.confirming) {
        screen_y = to_screen_y(logical_y);
        if (screen_y >= 0 && screen_y < viewport) {
            if (app.add.mode == AddMode::EditFromTicker)
                mvprintw(screen_y, 0, "confirm overwrite? [y/n]");
            else
                mvprintw(screen_y, 0, "confirm? [y/n]");
        }
        curs_set(0);
    }
    else {
        const int line_y = app.add.layout_y[app.add.index];
        const FieldKey current_key =
            fields[static_cast<std::size_t>(app.add.index)].key;
        const int cursor_column =
            (current_key == FieldKey::Ticker || !two_column_mode_active)
                ? 0
                : app.add.column;
        const int active_input_x =
            add_input_x_for_column(input_x, cursor_column);
        const int cursor_x =
            std::min(active_input_x + app.add.cursor, std::max(0, COLS - 1));
        if (line_y >= 0 && line_y < viewport) {
            desired_cursor_y = line_y;
            desired_cursor_x = cursor_x;
        }
    }

    int bottom_status_y = -1;
    if (total_lines > LINES && LINES > 0) {
        mvprintw(LINES - 1,
                 0,
                 "auto-scroll (%d/%d)",
                 app.add.scroll + 1,
                 max_scroll + 1);
        bottom_status_y = LINES - 1;
    }

    if (app.settings.show_help && LINES > 0 && total_lines < LINES) {
        const int hint_row_2 =
            (bottom_status_y == LINES - 1) ? (LINES - 2) : (LINES - 1);
        const int hint_row_1 = hint_row_2 - 1;
        std::string hint_top = "enter: confirm   esc: cancel";
        std::string hint_bottom;
        if (app.add.mode == AddMode::Create) {
            if (app.add.ticker_type_locked) {
                if (is_type1_add_variant(app.add.ticker_type))
                    hint_bottom = "space: switch 1(a)/1(b)";
                else
                    hint_bottom = "space: locked by ticker";
            }
            else {
                hint_bottom = "space: switch type";
            }
        }
        else {
            hint_bottom = "space: type locked";
        }
        if (two_column_allowed && !too_narrow_for_two_column) {
            hint_bottom += "   >: 2 cols   <: 1 col";
        }
        const int min_cols_for_hints =
            std::max(static_cast<int>(hint_top.size()),
                     static_cast<int>(hint_bottom.size())) +
            1;
        if (COLS < min_cols_for_hints) {
            hint_bottom.clear();
            hint_top.clear();
        }

        if (!hint_top.empty()) {
            attron(A_DIM);
            if (hint_row_1 >= 0) {
                mvprintw(hint_row_1,
                         0,
                         "%.*s",
                         std::max(0, COLS - 1),
                         hint_top.c_str());
                mvprintw(hint_row_2,
                         0,
                         "%.*s",
                         std::max(0, COLS - 1),
                         hint_bottom.c_str());
            }
            else if (hint_row_2 >= 0) {
                mvprintw(hint_row_2,
                         0,
                         "%.*s",
                         std::max(0, COLS - 1),
                         hint_top.c_str());
            }
            attroff(A_DIM);
        }
    }

    // Move the cursor after drawing hints/status so it stays on the active
    // input.
    if (!app.add.confirming && desired_cursor_y >= 0 &&
        desired_cursor_y < viewport) {
        render_blinking_add_input_caret(
            app, desired_cursor_y, desired_cursor_x);
    }

    wnoutrefresh(stdscr);
    doupdate();
}

inline std::optional<std::int64_t> as_i64_opt(const AddState::OptValue& v)
{
    if (!v) return std::nullopt;
    if (!std::holds_alternative<std::int64_t>(*v)) return std::nullopt;
    return std::get<std::int64_t>(*v);
}

inline std::optional<double> as_f64_opt(const AddState::OptValue& v)
{
    if (!v) return std::nullopt;
    if (!std::holds_alternative<double>(*v)) return std::nullopt;
    return std::get<double>(*v);
}

inline std::optional<std::string> as_str_opt(const AddState::OptValue& v)
{
    if (!v) return std::nullopt;
    if (!std::holds_alternative<std::string>(*v)) return std::nullopt;
    return std::get<std::string>(*v);
}

inline const AddState::OptValue*
add_value_for_key(const std::vector<AddState::OptValue>& values,
                  int ticker_type,
                  FieldKey key)
{
    const int idx = add_field_index(ticker_type, key);
    if (idx < 0 || idx >= static_cast<int>(values.size())) return nullptr;
    return &values[static_cast<std::size_t>(idx)];
}

inline bool
add_has_confirmable_period(const std::vector<AddState::OptValue>& values,
                           int ticker_type)
{
    const AddState::OptValue* ticker_v =
        add_value_for_key(values, ticker_type, FieldKey::Ticker);
    const AddState::OptValue* period_v =
        add_value_for_key(values, ticker_type, FieldKey::Period);
    const auto ticker_opt = ticker_v ? as_str_opt(*ticker_v) : std::nullopt;
    const auto period_opt = period_v ? as_str_opt(*period_v) : std::nullopt;
    return ticker_opt.has_value() && !ticker_opt->empty() &&
           period_opt.has_value() && !period_opt->empty();
}

inline bool
add_submit_parsed_period(AppState& app,
                         const std::vector<AddState::OptValue>& values,
                         std::string* out_ticker,
                         std::string* out_period)
{
    const AddState::OptValue* ticker_v =
        add_value_for_key(values, app.add.ticker_type, FieldKey::Ticker);
    const AddState::OptValue* period_v =
        add_value_for_key(values, app.add.ticker_type, FieldKey::Period);
    auto ticker_opt = ticker_v ? as_str_opt(*ticker_v) : std::nullopt;
    auto period_opt = period_v ? as_str_opt(*period_v) : std::nullopt;
    if (!ticker_opt || !period_opt) {
        route_error(app, "ticker/period missing");
        return false;
    }

    db::Database::FinancePayload payload{};
    auto i64_for = [&](FieldKey key) -> std::optional<std::int64_t> {
        const AddState::OptValue* v =
            add_value_for_key(values, app.add.ticker_type, key);
        return v ? as_i64_opt(*v) : std::nullopt;
    };
    auto f64_for = [&](FieldKey key) -> std::optional<double> {
        const AddState::OptValue* v =
            add_value_for_key(values, app.add.ticker_type, key);
        return v ? as_f64_opt(*v) : std::nullopt;
    };

    payload.net_income = i64_for(FieldKey::NetIncome);
    payload.eps = f64_for(FieldKey::Eps);

    if (app.add.ticker_type == kAddTickerType2) {
        payload.total_loans = i64_for(FieldKey::TotalLoans);
        payload.goodwill = i64_for(FieldKey::Goodwill);
        payload.total_assets = i64_for(FieldKey::TotalAssets);
        payload.total_deposits = i64_for(FieldKey::TotalDeposits);
        payload.total_liabilities = i64_for(FieldKey::TotalLiabilities);
        payload.net_interest_income = i64_for(FieldKey::NetInterestIncome);
        payload.non_interest_income = i64_for(FieldKey::NonInterestIncome);
        payload.loan_loss_provisions = i64_for(FieldKey::LoanLossProvisions);
        payload.non_interest_expense = i64_for(FieldKey::NonInterestExpense);
        payload.risk_weighted_assets = i64_for(FieldKey::RiskWeightedAssets);
        payload.common_equity_tier1 = i64_for(FieldKey::CommonEquityTier1);
        payload.net_charge_offs = i64_for(FieldKey::NetChargeOffs);
        payload.non_performing_loans = i64_for(FieldKey::NonPerformingLoans);
    }
    else if (app.add.ticker_type == kAddTickerType3) {
        payload.total_assets = i64_for(FieldKey::TotalAssets);
        payload.insurance_reserves = i64_for(FieldKey::InsuranceReserves);
        payload.total_debt = i64_for(FieldKey::TotalDebt);
        payload.total_liabilities = i64_for(FieldKey::TotalLiabilities);
        payload.earned_premiums = i64_for(FieldKey::EarnedPremiums);
        payload.claims_incurred = i64_for(FieldKey::ClaimsIncurred);
        payload.interest_expenses = i64_for(FieldKey::InterestExpenses);
        payload.total_expenses = i64_for(FieldKey::TotalExpenses);
        if (payload.total_expenses.has_value() &&
            payload.claims_incurred.has_value()) {
            payload.underwriting_expenses =
                *payload.total_expenses - *payload.claims_incurred -
                payload.interest_expenses.value_or(0);
        }
    }
    else {
        payload.cash_and_equivalents = i64_for(FieldKey::CashAndEquivalents);
        payload.current_assets = i64_for(FieldKey::CurrentAssets);
        payload.current_liabilities = i64_for(FieldKey::CurrentLiabilities);
        if (app.add.ticker_type == kAddTickerType1B) {
            const auto total_assets = i64_for(FieldKey::TotalAssets);
            if (payload.current_assets.has_value() &&
                total_assets.has_value()) {
                payload.non_current_assets =
                    *total_assets - *payload.current_assets;
            }
            else {
                payload.non_current_assets = std::nullopt;
            }

            const auto total_liabilities = i64_for(FieldKey::TotalLiabilities);
            if (payload.current_liabilities.has_value() &&
                total_liabilities.has_value()) {
                payload.non_current_liabilities =
                    *total_liabilities - *payload.current_liabilities;
            }
            else {
                payload.non_current_liabilities = std::nullopt;
            }
        }
        else {
            payload.non_current_assets = i64_for(FieldKey::NonCurrentAssets);
            payload.non_current_liabilities =
                i64_for(FieldKey::NonCurrentLiabilities);
        }
        payload.revenue = i64_for(FieldKey::Revenue);
        payload.cash_flow_from_operations = i64_for(FieldKey::CfoOperations);
        payload.cash_flow_from_investing = i64_for(FieldKey::CfiInvesting);
        payload.cash_flow_from_financing = i64_for(FieldKey::CffFinancing);
    }

    const int db_ticker_type = add_ticker_type_to_db_type(app.add.ticker_type);
    std::string err;
    if (!app.db->add_finances(
            *ticker_opt, *period_opt, payload, &err, db_ticker_type)) {
        route_error(app, err);
        return false;
    }

    if (out_ticker) *out_ticker = *ticker_opt;
    if (out_period) *out_period = *period_opt;
    return true;
}

inline bool handle_key_add(AppState& app, int ch)
{
    ensure_add_initialized(app);

    const auto& fields = add_fields_for_type(app.add.ticker_type);
    clamp_add_index(app, static_cast<int>(fields.size()));
    clamp_add_cursor(app);
    const int input_x = add_input_x_for_column(add_input_x(app), 0);
    const bool two_column_allowed = app.add.mode == AddMode::Create;
    const bool too_narrow_for_two_column =
        add_terminal_too_narrow_for_two_column_mode(COLS, input_x);
    const bool two_column_mode_active = two_column_allowed &&
                                        app.add.value_columns > 1 &&
                                        !too_narrow_for_two_column;
    if (!two_column_mode_active && app.add.column > 0) app.add.column = 0;

    if (ch == 27 /*ESC*/) {
        app.add.active = false;
        app.current = (app.add.mode == AddMode::EditFromTicker)
                          ? views::ViewId::Ticker
                          : views::ViewId::Home;
        return true;
    }

    if (app.add.confirming) {
        if (ch == 'y' || ch == 'Y' || ch == '\n' || ch == '\r' ||
            ch == KEY_ENTER) {
            std::string last_ticker;
            std::string last_period;
            if (!add_submit_parsed_period(
                    app, app.add.values, &last_ticker, &last_period)) {
                return true;
            }
            if (add_has_confirmable_period(app.add.values_extra,
                                           app.add.ticker_type)) {
                if (!add_submit_parsed_period(app,
                                              app.add.values_extra,
                                              &last_ticker,
                                              &last_period)) {
                    return true;
                }
            }

            app.tickers.invalidate_prefetch(); // refresh home ordering

            if (app.add.mode == AddMode::EditFromTicker) {
                std::string err;
                auto refreshed = app.db->get_finances(last_ticker, &err);
                if (!err.empty()) {
                    route_error(app, err);
                    return true;
                }
                if (refreshed.empty()) {
                    app.add.active = false;
                    app.current = views::ViewId::Home;
                    return true;
                }

                app.ticker_view.reset(
                    last_ticker, std::move(refreshed), app.add.ticker_type);
                const int idx =
                    add_find_period_index(app.ticker_view.rows, last_period);
                if (idx >= 0) app.ticker_view.index = idx;

                app.add.active = false;
                app.current = views::ViewId::Ticker;
                return true;
            }

            app.add.active = false;
            app.current = views::ViewId::Home;
            return true;
        }

        if (ch == 'n' || ch == 'N') {
            app.add.confirming = false;
            return true;
        }

        return true;
    }

    const int BACKSPACE_1 = KEY_BACKSPACE;
    const int BACKSPACE_2 = 127;
    const int BACKSPACE_3 = 8;

    if (ch == ' ') {
        cycle_add_type_and_clear(app);
        return true;
    }

    if (ch == '>') {
        if (!two_column_allowed) return true;
        if (too_narrow_for_two_column) return true;
        if (app.add.value_columns < 2) app.add.value_columns = 2;
        return true;
    }

    if (ch == '<') {
        if (!two_column_allowed) return true;
        if (too_narrow_for_two_column) return true;
        if (app.add.value_columns > 1) {
            app.add.value_columns = 1;
            app.add.column = 0;
            for (auto& buf : app.add.buffers_extra) {
                buf.clear();
            }
            std::fill(app.add.values_extra.begin(),
                      app.add.values_extra.end(),
                      std::nullopt);
            clamp_add_cursor(app);
        }
        return true;
    }

    if (ch == '\t'
#ifdef KEY_TAB
        || ch == KEY_TAB
#endif
    ) {
        if (app.add.index + 1 < static_cast<int>(fields.size()))
            app.add.index += 1;
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        if (key == FieldKey::Ticker && app.add.column > 0) app.add.column = 0;
        clamp_add_cursor(app);
        return true;
    }

    if (ch == KEY_UP) {
        if (app.add.index > 0) app.add.index -= 1;
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        if (key == FieldKey::Ticker && app.add.column > 0) app.add.column = 0;
        clamp_add_cursor(app);
        return true;
    }

    if (ch == KEY_DOWN) {
        if (app.add.index + 1 < static_cast<int>(fields.size()))
            app.add.index += 1;
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        if (key == FieldKey::Ticker && app.add.column > 0) app.add.column = 0;
        clamp_add_cursor(app);
        return true;
    }

    if (ch == KEY_LEFT) {
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        if (key != FieldKey::Ticker && two_column_mode_active) {
            if (app.add.cursor > 0) {
                app.add.cursor -= 1;
                return true;
            }
            if (app.add.column > 0) {
                app.add.column -= 1;
                clamp_add_cursor(app);
            }
            return true;
        }
        if (app.add.cursor > 0) app.add.cursor -= 1;
        return true;
    }

    if (ch == KEY_RIGHT) {
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        const auto& active_buffers =
            (app.add.column <= 0) ? app.add.buffers : app.add.buffers_extra;
        const auto& current =
            active_buffers[static_cast<std::size_t>(app.add.index)];
        if (key != FieldKey::Ticker && two_column_mode_active) {
            if (app.add.cursor < static_cast<int>(current.size())) {
                app.add.cursor += 1;
                return true;
            }
            if (app.add.column + 1 < app.add.value_columns) {
                app.add.column += 1;
                clamp_add_cursor(app);
            }
            return true;
        }
        if (app.add.cursor < static_cast<int>(current.size()))
            app.add.cursor += 1;
        return true;
    }

    if (ch == BACKSPACE_1 || ch == BACKSPACE_2 || ch == BACKSPACE_3) {
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        if (key == FieldKey::Ticker && app.add.column > 0) return true;
        auto& active_buffers =
            (app.add.column <= 0) ? app.add.buffers : app.add.buffers_extra;
        auto& current = active_buffers[static_cast<std::size_t>(app.add.index)];
        if (app.add.cursor > 0 && !current.empty()) {
            current.erase(static_cast<std::size_t>(app.add.cursor - 1), 1);
            app.add.cursor -= 1;
            normalize_field_buffer(key, current, &app.add.cursor);
            if (key == FieldKey::Ticker && app.add.column == 0) {
                sync_add_secondary_ticker(app);
                return sync_add_type_lock_from_ticker(app);
            }
        }
        return true;
    }

    if (ch == KEY_DC) {
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        if (key == FieldKey::Ticker && app.add.column > 0) return true;
        auto& active_buffers =
            (app.add.column <= 0) ? app.add.buffers : app.add.buffers_extra;
        auto& current = active_buffers[static_cast<std::size_t>(app.add.index)];
        if (app.add.cursor >= 0 &&
            app.add.cursor < static_cast<int>(current.size())) {
            current.erase(static_cast<std::size_t>(app.add.cursor), 1);
            normalize_field_buffer(key, current, &app.add.cursor);
            if (key == FieldKey::Ticker && app.add.column == 0) {
                sync_add_secondary_ticker(app);
                return sync_add_type_lock_from_ticker(app);
            }
        }
        return true;
    }

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        std::vector<AddState::OptValue> parsed_primary;
        std::vector<AddState::OptValue> parsed_secondary;
        int invalid_index = -1;

        if (!validate_add_form(
                fields, app.add.buffers, &parsed_primary, &invalid_index)) {
            app.add.index = std::max(0, invalid_index);
            app.add.column = 0;
            auto& invalid_buf =
                app.add.buffers[static_cast<std::size_t>(app.add.index)];
            invalid_buf.clear();
            app.add.cursor = 0;
            flash_add_invalid_marker(app, app.add.index, app.add.column);
            return true;
        }

        if (two_column_mode_active && !validate_add_form(fields,
                                                         app.add.buffers_extra,
                                                         &parsed_secondary,
                                                         &invalid_index)) {
            app.add.index = std::max(0, invalid_index);
            app.add.column = 1;
            auto& invalid_buf =
                app.add.buffers_extra[static_cast<std::size_t>(app.add.index)];
            invalid_buf.clear();
            app.add.cursor = 0;
            flash_add_invalid_marker(app, app.add.index, app.add.column);
            return true;
        }

        app.add.values = std::move(parsed_primary);
        if (two_column_mode_active) {
            app.add.values_extra = std::move(parsed_secondary);
        }
        else {
            std::fill(app.add.values_extra.begin(),
                      app.add.values_extra.end(),
                      std::nullopt);
        }
        app.add.confirming = true;
        return true;
    }

    auto& active_buffers =
        (app.add.column <= 0) ? app.add.buffers : app.add.buffers_extra;
    auto& current = active_buffers[static_cast<std::size_t>(app.add.index)];
    const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
    if (key == FieldKey::Ticker && app.add.column > 0) return true;

    if (is_allowed_char_for_current_field(ch, key, current, app.add.cursor)) {
        current.insert(
            static_cast<std::size_t>(app.add.cursor), 1, static_cast<char>(ch));
        app.add.cursor += 1;
        normalize_field_buffer(key, current, &app.add.cursor);
        if (key == FieldKey::Ticker && app.add.column == 0) {
            sync_add_secondary_ticker(app);
            return sync_add_type_lock_from_ticker(app);
        }
        return true;
    }

    // swallow disallowed chars
    return true;
}

} // namespace views


