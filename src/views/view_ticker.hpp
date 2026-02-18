#pragma once

#include "views/view_ticker_helpers.hpp"

namespace views {

inline void render_ticker(AppState& app)
{
    curs_set(1);
    erase();

    auto& view = app.ticker_view;
    view.clamp_index();
    if (view.input_index < 0) view.input_index = 0;
    if (view.input_index > 1) view.input_index = 1;
    if (view.scroll < 0) view.scroll = 0;

    int help_lines = 0;
    if (app.settings.show_help) {
        if (LINES >= 9)
            help_lines = 4;
        else if (LINES >= 7)
            help_lines = 2;
    }

    if (LINES > 0) {
        if (has_colors()) attron(COLOR_PAIR(kColorPairHeader));
        attron(A_BOLD);
        mvprintw(0, 0, "intrinsic ~");
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(kColorPairHeader));
        if (COLS > 11) mvprintw(0, 11, " %s", view.ticker.c_str());
    }
    if (LINES > 2 && !view.status_line.empty()) {
        mvprintw(2, 0, "%.*s", std::max(0, COLS - 1), view.status_line.c_str());
    }

    if (view.rows.empty()) {
        curs_set(0);
        if (LINES > 2) mvprintw(2, 0, "no data for ticker");
        if (help_lines >= 2) {
            attron(A_DIM);
            mvprintw(LINES - 2, 0, "x: delete   e: edit   c: copy");
            mvprintw(LINES - 1, 0, "h: home   ?: help   s: settings   q: quit");
            attroff(A_DIM);
        }
        wnoutrefresh(stdscr);
        doupdate();
        return;
    }

    const auto& row = view.rows[view.index];
    const db::Database::FinanceRow* previous_row =
        find_previous_year_same_period(view.all_rows, row);
    const std::string period = period_label(row);
    if (LINES > 1) {
        mvprintw(1,
                 0,
                 "period: %s (%d/%d)  view: %s",
                 period.c_str(),
                 view.index + 1,
                 static_cast<int>(view.rows.size()),
                 view.yearly_only ? "yearly" : "all");
    }

    const auto total_assets =
        add_i64(row.current_assets, row.non_current_assets);
    const auto total_liabilities =
        add_i64(row.current_liabilities, row.non_current_liabilities);
    const auto equity = sub_i64(total_assets, total_liabilities);
    const auto working_capital =
        sub_i64(row.current_assets, row.current_liabilities);

    const auto net_income_d = to_f64(row.net_income);
    const auto revenue_d = to_f64(row.revenue);
    const auto total_assets_d = to_f64(total_assets);
    const auto total_liabilities_d = to_f64(total_liabilities);
    const auto equity_d = to_f64(equity);
    const auto current_assets_d = to_f64(row.current_assets);
    const auto non_current_assets_d = to_f64(row.non_current_assets);
    const auto current_liabilities_d = to_f64(row.current_liabilities);
    const auto non_current_liabilities_d = to_f64(row.non_current_liabilities);
    const auto working_capital_d = to_f64(working_capital);
    const auto cash_d = to_f64(row.cash_and_equivalents);
    const auto cash_flow_ops_d_current = to_f64(row.cash_flow_from_operations);
    const auto eps_d_current = row.eps;
    const auto prev_cash_d = previous_row
                                 ? to_f64(previous_row->cash_and_equivalents)
                                 : std::nullopt;
    const auto prev_current_assets_d =
        previous_row ? to_f64(previous_row->current_assets) : std::nullopt;
    const auto prev_non_current_assets_d =
        previous_row ? to_f64(previous_row->non_current_assets) : std::nullopt;
    const auto prev_current_liabilities_d =
        previous_row ? to_f64(previous_row->current_liabilities) : std::nullopt;
    const auto prev_non_current_liabilities_d =
        previous_row ? to_f64(previous_row->non_current_liabilities)
                     : std::nullopt;
    const auto prev_revenue_d =
        previous_row ? to_f64(previous_row->revenue) : std::nullopt;
    const auto prev_net_income_d =
        previous_row ? to_f64(previous_row->net_income) : std::nullopt;
    const auto prev_eps_d = previous_row ? previous_row->eps : std::nullopt;
    const auto prev_cash_flow_ops_d =
        previous_row ? to_f64(previous_row->cash_flow_from_operations)
                     : std::nullopt;
    const auto prev_cash_flow_inv_d =
        previous_row ? to_f64(previous_row->cash_flow_from_investing)
                     : std::nullopt;
    const auto prev_cash_flow_fin_d =
        previous_row ? to_f64(previous_row->cash_flow_from_financing)
                     : std::nullopt;
    const auto prev_total_assets =
        previous_row ? add_i64(previous_row->current_assets,
                               previous_row->non_current_assets)
                     : std::nullopt;
    const auto prev_total_liabilities =
        previous_row ? add_i64(previous_row->current_liabilities,
                               previous_row->non_current_liabilities)
                     : std::nullopt;
    const auto prev_equity = sub_i64(prev_total_assets, prev_total_liabilities);
    const auto prev_working_capital =
        previous_row ? sub_i64(previous_row->current_assets,
                               previous_row->current_liabilities)
                     : std::nullopt;
    const auto prev_total_assets_d = to_f64(prev_total_assets);
    const auto prev_total_liabilities_d = to_f64(prev_total_liabilities);
    const auto prev_equity_d = to_f64(prev_equity);
    const auto prev_working_capital_d = to_f64(prev_working_capital);

    const char family = period_family(row);
    const int ttm_window = ttm_window_for_family(family);
    const bool ttm_family_supported = ttm_window > 0;

    std::optional<double> ttm_eps;
    std::optional<double> ttm_net_income_d;
    std::optional<double> ttm_cash_flow_ops_d;

    if (ttm_family_supported) {
        const std::string selected_period = period_label(row);
        const int all_index = find_period_index(view.all_rows, selected_period);

        ttm_eps = ttm_sum_for_family(
            view.all_rows,
            all_index,
            family,
            ttm_window,
            [](const db::Database::FinanceRow& r) { return r.eps; });

        ttm_net_income_d =
            ttm_sum_for_family(view.all_rows,
                               all_index,
                               family,
                               ttm_window,
                               [](const db::Database::FinanceRow& r) {
                                   return to_f64(r.net_income);
                               });

        ttm_cash_flow_ops_d =
            ttm_sum_for_family(view.all_rows,
                               all_index,
                               family,
                               ttm_window,
                               [](const db::Database::FinanceRow& r) {
                                   return to_f64(r.cash_flow_from_operations);
                               });
    }

    const bool prefer_ttm_for_derived =
        app.settings.ttm && ttm_family_supported;
    const bool prefer_ttm_for_wished = app.settings.ttm && ttm_family_supported;

    const auto eps_for_derived =
        (prefer_ttm_for_derived && is_valid_number(ttm_eps)) ? ttm_eps
                                                             : eps_d_current;
    const auto net_income_for_derived =
        (prefer_ttm_for_derived && is_valid_number(ttm_net_income_d))
            ? ttm_net_income_d
            : net_income_d;
    const auto cash_flow_ops_for_derived =
        (prefer_ttm_for_derived && is_valid_number(ttm_cash_flow_ops_d))
            ? ttm_cash_flow_ops_d
            : cash_flow_ops_d_current;

    const auto eps_for_wished = (prefer_ttm_for_wished && ttm_eps.has_value() &&
                                 std::isfinite(*ttm_eps) && *ttm_eps > 0.0)
                                    ? ttm_eps
                                    : eps_d_current;
    const auto net_income_for_wished =
        (prefer_ttm_for_wished && ttm_net_income_d.has_value() &&
         std::isfinite(*ttm_net_income_d) && *ttm_net_income_d > 0.0)
            ? ttm_net_income_d
            : net_income_d;

    const auto net_margin = div_opt_nonzero(net_income_d, revenue_d);
    const auto roa = div_opt_nonzero(net_income_d, total_assets_d);
    const auto roe = div_opt_nonzero(net_income_d, equity_d);

    const auto liquidity =
        div_opt_nonzero(current_assets_d, current_liabilities_d);
    const auto solvency = div_opt_nonzero(total_assets_d, total_liabilities_d);
    const auto leverage = div_opt_nonzero(total_liabilities_d, equity_d);
    const auto wc_over_non_current =
        div_opt_nonzero(working_capital_d, non_current_liabilities_d);
    const auto shares_approx_raw =
        div_opt_nonzero(net_income_for_derived, eps_for_derived);
    const auto shares_approx =
        shares_approx_raw.has_value()
            ? std::optional<double>(std::round(*shares_approx_raw))
            : std::nullopt;
    const auto book_value = div_opt_nonzero(equity_d, shares_approx);
    const auto prev_wc_over_non_current =
        div_opt_nonzero(prev_working_capital_d, prev_non_current_liabilities_d);
    const auto prev_shares_approx_raw =
        div_opt_nonzero(prev_net_income_d, prev_eps_d);
    const auto prev_shares_approx =
        prev_shares_approx_raw.has_value()
            ? std::optional<double>(std::round(*prev_shares_approx_raw))
            : std::nullopt;
    const auto prev_book_value =
        div_opt_nonzero(prev_equity_d, prev_shares_approx);
    const auto prev_net_margin =
        div_opt_nonzero(prev_net_income_d, prev_revenue_d);
    const auto prev_roa =
        div_opt_nonzero(prev_net_income_d, prev_total_assets_d);
    const auto prev_roe = div_opt_nonzero(prev_net_income_d, prev_equity_d);
    const auto prev_liquidity =
        div_opt_nonzero(prev_current_assets_d, prev_current_liabilities_d);
    const auto prev_solvency =
        div_opt_nonzero(prev_total_assets_d, prev_total_liabilities_d);
    const auto prev_leverage =
        div_opt_nonzero(prev_total_liabilities_d, prev_equity_d);

    const std::optional<double> typed_price =
        parse_decimal_input(view.inputs[0]);
    const std::optional<double> wished_per =
        parse_decimal_input(view.inputs[1]);
    const auto ratio_price = null_if_zero_or_invalid(typed_price);
    const auto ratio_total_liabilities =
        null_if_zero_or_invalid(total_liabilities_d);
    const auto ratio_cash = null_if_zero_or_invalid(cash_d);
    const auto prev_ratio_total_liabilities =
        null_if_zero_or_invalid(prev_total_liabilities_d);
    const auto prev_ratio_cash = null_if_zero_or_invalid(prev_cash_d);

    const auto market_cap = mul_opt_nonzero(ratio_price, shares_approx);
    const auto enterprise_value =
        (market_cap.has_value() && ratio_total_liabilities.has_value() &&
         ratio_cash.has_value())
            ? std::optional<double>(*market_cap + *ratio_total_liabilities -
                                    *ratio_cash)
            : std::nullopt;
    const auto ev_over_cash_flow_ops_raw =
        div_opt_nonzero(enterprise_value, cash_flow_ops_for_derived);
    const auto per_ratio = div_opt_nonzero(ratio_price, eps_for_derived);
    const auto price_to_book = div_opt_nonzero(ratio_price, book_value);
    const auto ev_over_market_cap_raw =
        div_opt_nonzero(enterprise_value, market_cap);
    const auto ev_over_net_income_raw =
        div_opt_nonzero(enterprise_value, net_income_for_derived);
    const auto ev_over_cash_flow_ops =
        null_if_negative(ev_over_cash_flow_ops_raw);
    const auto ev_over_market_cap = null_if_negative(ev_over_market_cap_raw);
    const auto ev_over_net_income = null_if_negative(ev_over_net_income_raw);
    const auto prev_market_cap =
        mul_opt_nonzero(ratio_price, prev_shares_approx);
    const auto prev_enterprise_value =
        (prev_market_cap.has_value() &&
         prev_ratio_total_liabilities.has_value() &&
         prev_ratio_cash.has_value())
            ? std::optional<double>(*prev_market_cap +
                                    *prev_ratio_total_liabilities -
                                    *prev_ratio_cash)
            : std::nullopt;
    const auto prev_ev_over_cash_flow_ops_raw =
        div_opt_nonzero(prev_enterprise_value, prev_cash_flow_ops_d);
    const auto prev_per_ratio = div_opt_nonzero(ratio_price, prev_eps_d);
    const auto prev_price_to_book =
        div_opt_nonzero(ratio_price, prev_book_value);
    const auto prev_ev_over_market_cap_raw =
        div_opt_nonzero(prev_enterprise_value, prev_market_cap);
    const auto prev_ev_over_net_income_raw =
        div_opt_nonzero(prev_enterprise_value, prev_net_income_d);
    const auto prev_ev_over_cash_flow_ops =
        null_if_negative(prev_ev_over_cash_flow_ops_raw);
    const auto prev_ev_over_market_cap =
        null_if_negative(prev_ev_over_market_cap_raw);
    const auto prev_ev_over_net_income =
        null_if_negative(prev_ev_over_net_income_raw);
    std::optional<double> score = std::nullopt;

    const auto price_needed_for_wished_per =
        rounded_price_for_wished_per(wished_per, eps_for_wished, eps_d_current);
    const auto required_eps = div_opt(typed_price, wished_per);
    const auto shares_for_wished =
        div_opt(net_income_for_wished, eps_for_wished);
    const auto required_net_income = mul_opt(required_eps, shares_for_wished);
    const auto price_needed_change =
        percent_change(price_needed_for_wished_per, typed_price);
    const auto required_net_income_change = required_net_income_change_pct(
        required_net_income, net_income_for_wished);

    if (ratio_price.has_value()) {
        const bool missing_required =
            !is_valid_number(net_income_for_derived) ||
            !is_valid_number(shares_approx) ||
            !is_valid_number(ev_over_market_cap_raw) ||
            !is_valid_number(price_to_book);

        if (!missing_required) {
            const bool non_positive_base =
                (eps_for_derived.has_value() && *eps_for_derived <= 0.0) ||
                (book_value.has_value() && *book_value <= 0.0) ||
                (cash_flow_ops_for_derived.has_value() &&
                 *cash_flow_ops_for_derived <= 0.0) ||
                (net_income_for_derived.has_value() &&
                 *net_income_for_derived <= 0.0);

            if (non_positive_base) {
                score = 0.0;
            }
            else if (enterprise_value.has_value() && *enterprise_value <= 0.0) {
                score = 10.0;
            }
            else {
                const auto r2 = ratio_score(per_ratio, 50.0);
                const auto r3 = ratio_score(price_to_book, 20.0);
                if (r2.has_value() && r3.has_value()) {
                    if (ev_over_cash_flow_ops_raw.has_value()) {
                        const auto r1 =
                            ratio_score(ev_over_cash_flow_ops_raw, 50.0);
                        if (r1.has_value())
                            score = 0.4 * (*r1) + 0.3 * (*r2) + 0.3 * (*r3);
                    }
                    else {
                        score = 0.5 * (*r2) + 0.5 * (*r3);
                    }
                }
            }
        }
    }

    std::optional<double> prev_score = std::nullopt;
    if (ratio_price.has_value()) {
        const bool missing_required =
            !is_valid_number(prev_net_income_d) ||
            !is_valid_number(prev_shares_approx) ||
            !is_valid_number(prev_ev_over_market_cap_raw) ||
            !is_valid_number(prev_price_to_book);

        if (!missing_required) {
            const bool non_positive_base =
                (prev_eps_d.has_value() && *prev_eps_d <= 0.0) ||
                (prev_book_value.has_value() && *prev_book_value <= 0.0) ||
                (prev_cash_flow_ops_d.has_value() &&
                 *prev_cash_flow_ops_d <= 0.0) ||
                (prev_net_income_d.has_value() && *prev_net_income_d <= 0.0);

            if (non_positive_base) {
                prev_score = 0.0;
            }
            else if (prev_enterprise_value.has_value() &&
                     *prev_enterprise_value <= 0.0) {
                prev_score = 10.0;
            }
            else {
                const auto r2 = ratio_score(prev_per_ratio, 50.0);
                const auto r3 = ratio_score(prev_price_to_book, 20.0);
                if (r2.has_value() && r3.has_value()) {
                    if (prev_ev_over_cash_flow_ops_raw.has_value()) {
                        const auto r1 =
                            ratio_score(prev_ev_over_cash_flow_ops_raw, 50.0);
                        if (r1.has_value()) {
                            prev_score =
                                0.4 * (*r1) + 0.3 * (*r2) + 0.3 * (*r3);
                        }
                    }
                    else {
                        prev_score = 0.5 * (*r2) + 0.5 * (*r3);
                    }
                }
            }
        }
    }

    const std::vector<Metric> valuation_box = {
        {"P / E",
         with_change(format_ratio_opt(per_ratio, kNaValue),
                     ratio_percent_change(per_ratio, prev_per_ratio)),
         true,
         true},
        {"P / BV",
         with_change(format_ratio_opt(price_to_book, kNaValue),
                     ratio_percent_change(price_to_book, prev_price_to_book)),
         true,
         true},
        {"EV",
         with_change(
             format_compact_i64_from_f64_opt(enterprise_value, kNaValue),
             percent_change(enterprise_value, prev_enterprise_value)),
         false,
         true},
        {"EVcap",
         with_change(
             format_ratio_opt(ev_over_market_cap, kNaValue),
             ratio_percent_change(ev_over_market_cap, prev_ev_over_market_cap)),
         true,
         true},
        {"EV / CFop",
         with_change(format_ratio_opt(ev_over_cash_flow_ops, kNaValue),
                     ratio_percent_change(ev_over_cash_flow_ops,
                                          prev_ev_over_cash_flow_ops)),
         true,
         true},
        {"EV / NP",
         with_change(
             format_ratio_opt(ev_over_net_income, kNaValue),
             ratio_percent_change(ev_over_net_income, prev_ev_over_net_income)),
         true,
         true},
    };

    const std::vector<Metric> balance_sheet_box = {
        {"CA",
         with_change(format_i64_opt(row.current_assets),
                     percent_change(current_assets_d, prev_current_assets_d))},
        {"NCA",
         with_change(
             format_i64_opt(row.non_current_assets),
             percent_change(non_current_assets_d, prev_non_current_assets_d))},
        {"Cash",
         with_change(format_i64_opt(row.cash_and_equivalents),
                     percent_change(cash_d, prev_cash_d))},
        {"TA",
         with_change(format_i64_opt(total_assets),
                     percent_change(total_assets_d, prev_total_assets_d))},
        {"CL",
         with_change(format_i64_opt(row.current_liabilities),
                     percent_change(current_liabilities_d,
                                    prev_current_liabilities_d))},
        {"NCL",
         with_change(format_i64_opt(row.non_current_liabilities),
                     percent_change(non_current_liabilities_d,
                                    prev_non_current_liabilities_d))},
        {"E",
         with_change(format_i64_opt(equity),
                     percent_change(equity_d, prev_equity_d))},
        {"TL",
         with_change(
             format_i64_opt(total_liabilities),
             percent_change(total_liabilities_d, prev_total_liabilities_d))},
        {"WC",
         with_change(
             format_i64_opt(working_capital),
             percent_change(working_capital_d, prev_working_capital_d))},
        {"WC / NCL",
         with_change(format_f64_opt(wc_over_non_current),
                     ratio_percent_change(wc_over_non_current,
                                          prev_wc_over_non_current))},
        {"Shs~",
         with_change(format_f64_integer_opt(shares_approx),
                     percent_change(shares_approx, prev_shares_approx))},
        {"BV",
         with_change(format_ratio_opt(book_value),
                     ratio_percent_change(book_value, prev_book_value))},
    };

    const std::vector<Metric> balance_sheet_box_single = {
        {"CA",
         with_change(format_i64_opt(row.current_assets),
                     percent_change(current_assets_d, prev_current_assets_d))},
        {"Cash",
         with_change(format_i64_opt(row.cash_and_equivalents),
                     percent_change(cash_d, prev_cash_d))},
        {"NCA",
         with_change(
             format_i64_opt(row.non_current_assets),
             percent_change(non_current_assets_d, prev_non_current_assets_d))},
        {"TA",
         with_change(format_i64_opt(total_assets),
                     percent_change(total_assets_d, prev_total_assets_d))},
        {"CL",
         with_change(format_i64_opt(row.current_liabilities),
                     percent_change(current_liabilities_d,
                                    prev_current_liabilities_d))},
        {"NCL",
         with_change(format_i64_opt(row.non_current_liabilities),
                     percent_change(non_current_liabilities_d,
                                    prev_non_current_liabilities_d))},
        {"TL",
         with_change(
             format_i64_opt(total_liabilities),
             percent_change(total_liabilities_d, prev_total_liabilities_d))},
        {"E",
         with_change(format_i64_opt(equity),
                     percent_change(equity_d, prev_equity_d))},
        {"WC",
         with_change(
             format_i64_opt(working_capital),
             percent_change(working_capital_d, prev_working_capital_d))},
        {"WC / NCL",
         with_change(format_f64_opt(wc_over_non_current),
                     ratio_percent_change(wc_over_non_current,
                                          prev_wc_over_non_current))},
        {"Shs~",
         with_change(format_f64_integer_opt(shares_approx),
                     percent_change(shares_approx, prev_shares_approx))},
        {"BV",
         with_change(format_ratio_opt(book_value),
                     ratio_percent_change(book_value, prev_book_value))},
    };

    const std::vector<Metric> score_box = {
        {"Score",
         with_change(format_ratio_opt(score, kNaValue),
                     ratio_percent_change(score, prev_score)),
         false,
         true},
        {"", ""},
        {"p needed",
         with_change(format_compact_i64_from_f64_opt(price_needed_for_wished_per,
                                                     kNaValue),
                     price_needed_change),
         false,
         true},
        {"NP needed",
         with_change(format_compact_i64_from_f64_opt(required_net_income,
                                                     kNaValue),
                     required_net_income_change),
         false,
         true},
    };

    const std::vector<Metric> performance_box = {
        {"R",
         with_change(format_i64_opt(row.revenue),
                     percent_change(revenue_d, prev_revenue_d))},
        {"NP",
         with_change(format_i64_opt(row.net_income),
                     percent_change(net_income_d, prev_net_income_d))},
        {"EPS",
         with_change(format_f64_opt(row.eps),
                     percent_change(row.eps, prev_eps_d))},
        {"Mnet",
         with_change(format_f64_opt(net_margin, true),
                     ratio_percent_change(net_margin, prev_net_margin))},
        {"ROA",
         with_change(format_f64_opt(roa, true),
                     ratio_percent_change(roa, prev_roa))},
        {"ROE",
         with_change(format_f64_opt(roe, true),
                     ratio_percent_change(roe, prev_roe))},
    };

    const std::vector<Metric> quality_cashflow_box = {
        {"Liq.",
         with_change(format_ratio_opt(liquidity),
                     ratio_percent_change(liquidity, prev_liquidity))},
        {"CFop",
         with_change(
             format_i64_opt(row.cash_flow_from_operations),
             percent_change(cash_flow_ops_d_current, prev_cash_flow_ops_d))},
        {"Sol.",
         with_change(format_ratio_opt(solvency),
                     ratio_percent_change(solvency, prev_solvency))},
        {"CFinv",
         with_change(format_i64_opt(row.cash_flow_from_investing),
                     percent_change(to_f64(row.cash_flow_from_investing),
                                    prev_cash_flow_inv_d))},
        {"Lev.",
         with_change(format_ratio_opt(leverage),
                     ratio_percent_change(leverage, prev_leverage))},
        {"CFfin",
         with_change(format_i64_opt(row.cash_flow_from_financing),
                     percent_change(to_f64(row.cash_flow_from_financing),
                                    prev_cash_flow_fin_d))},
    };

    const std::vector<Metric> quality_cashflow_box_single = {
        {"Liq.",
         with_change(format_ratio_opt(liquidity),
                     ratio_percent_change(liquidity, prev_liquidity))},
        {"Sol.",
         with_change(format_ratio_opt(solvency),
                     ratio_percent_change(solvency, prev_solvency))},
        {"Lev.",
         with_change(format_ratio_opt(leverage),
                     ratio_percent_change(leverage, prev_leverage))},
        {"", " "},
        {"CFop",
         with_change(
             format_i64_opt(row.cash_flow_from_operations),
             percent_change(cash_flow_ops_d_current, prev_cash_flow_ops_d))},
        {"CFinv",
         with_change(format_i64_opt(row.cash_flow_from_investing),
                     percent_change(to_f64(row.cash_flow_from_investing),
                                    prev_cash_flow_inv_d))},
        {"CFfin",
         with_change(format_i64_opt(row.cash_flow_from_financing),
                     percent_change(to_f64(row.cash_flow_from_financing),
                                    prev_cash_flow_fin_d))},
    };

    constexpr int body_top = 3;
    const int help_start = std::max(0, LINES - help_lines);
    int body_height = std::max(1, help_start - body_top);

    constexpr std::array<const char*, 2> labels = {"price", "wished per"};
    const int input_x = (COLS >= 28) ? 16 : 10;
    const int input_label_w = std::max(4, input_x - 5);

    constexpr int metric_col_gap = 1;
    constexpr int box_gap_rows = 1;
    constexpr int preferred_col_w = 28;
    // Keep enough room per column for sane max text:
    // longest label (9) + value (up to ~11 chars) + change ("12.3%").
    constexpr int min_label_w_for_two_col = 9;
    constexpr int min_value_w_for_two_col = 11;
    constexpr int min_change_w_for_two_col = 5;
    constexpr int min_two_col_w = min_label_w_for_two_col + 1 +
                                  min_value_w_for_two_col + 1 +
                                  min_change_w_for_two_col;
    const bool two_metric_cols = COLS >= (2 * min_two_col_w + metric_col_gap);

    const int col_w = [&] {
        if (!two_metric_cols) return std::max(12, COLS);
        const int usable_cols = std::max(0, COLS - metric_col_gap);
        return std::min(preferred_col_w, usable_cols / 2);
    }();
    const int c1_x = 0;
    const int c2_x = two_metric_cols ? (c1_x + col_w + metric_col_gap) : c1_x;
    const int label_w = std::clamp(col_w - 17, 6, 9);
    const auto& active_balance_sheet_box =
        two_metric_cols ? balance_sheet_box : balance_sheet_box_single;
    const auto& active_quality_cashflow_box =
        two_metric_cols ? quality_cashflow_box : quality_cashflow_box_single;

    const std::vector<std::vector<Metric>> metric_boxes = {
        score_box,
        valuation_box,
        active_balance_sheet_box,
        active_quality_cashflow_box,
        performance_box,
    };

    const int first_input_y = 0;
    const int second_input_y = 1;
    const int cursor_body_y =
        (view.input_index == 0) ? first_input_y : second_input_y;

    const int metrics_start_y = second_input_y + 2;
    const auto box_rows = [&](const std::vector<Metric>& box) {
        if (two_metric_cols) return static_cast<int>((box.size() + 1) / 2);

        int rows = 0;
        for (const auto& metric : box) {
            const bool is_placeholder =
                ((metric.label == nullptr || metric.label[0] == '\0') &&
                 metric.value.empty());
            if (!is_placeholder) rows += 1;
        }
        return rows;
    };
    int total_metric_rows = 0;
    for (std::size_t i = 0; i < metric_boxes.size(); ++i) {
        total_metric_rows += box_rows(metric_boxes[i]);
        if (i + 1 < metric_boxes.size()) total_metric_rows += box_gap_rows;
    }
    const int total_body_lines = metrics_start_y + total_metric_rows;

    const int max_scroll = std::max(0, total_body_lines - body_height);
    view.scroll = std::clamp(view.scroll, 0, max_scroll);
    if (cursor_body_y < view.scroll) view.scroll = cursor_body_y;
    if (cursor_body_y >= view.scroll + body_height) {
        view.scroll = cursor_body_y - body_height + 1;
    }
    view.scroll = std::clamp(view.scroll, 0, max_scroll);

    auto screen_y_for_body = [&](int logical_y) {
        return body_top + logical_y - view.scroll;
    };

    int cursor_y = std::clamp(
        screen_y_for_body(cursor_body_y), body_top, body_top + body_height - 1);
    int cursor_x =
        input_x + static_cast<int>(view.inputs[view.input_index].size());
    for (int i = 0; i < 2; ++i) {
        const int logical_y = first_input_y + i;
        const int screen_y = screen_y_for_body(logical_y);
        if (screen_y < body_top || screen_y >= body_top + body_height) continue;

        const bool selected = view.input_index == i;
        mvaddch(screen_y, 0, selected ? ('>' | A_BOLD) : ' ');
        mvprintw(screen_y, 2, "%-*s", input_label_w, labels[i]);
        const bool empty = view.inputs[i].empty();
        const std::string shown =
            (empty && !selected) ? kNaValue : view.inputs[i];
        if (shown == kNaValue) {
            attron(A_DIM);
            mvprintw(screen_y, input_x, "%s", shown.c_str());
            attroff(A_DIM);
        }
        else if (!shown.empty() && has_colors()) {
            attron(COLOR_PAIR(kColorPairInputValue));
            mvprintw(screen_y, input_x, "%s", shown.c_str());
            attroff(COLOR_PAIR(kColorPairInputValue));
        }
        else {
            mvprintw(screen_y, input_x, "%s", shown.c_str());
        }
    }

    auto render_box = [&](int start_y, const std::vector<Metric>& box) {
        if (two_metric_cols) {
            for (std::size_t i = 0; i < box.size(); ++i) {
                const int row_in_box = static_cast<int>(i / 2);
                const int x = (i % 2 == 0) ? c1_x : c2_x;
                render_metric_at(x,
                                 start_y + row_in_box,
                                 col_w,
                                 label_w,
                                 box[i],
                                 view.scroll,
                                 body_top,
                                 body_height);
            }
            return;
        }

        int row_offset = 0;
        for (const auto& metric : box) {
            const bool is_placeholder =
                ((metric.label == nullptr || metric.label[0] == '\0') &&
                 metric.value.empty());
            if (is_placeholder) continue;
            render_metric_at(c1_x,
                             start_y + row_offset,
                             col_w,
                             label_w,
                             metric,
                             view.scroll,
                             body_top,
                             body_height);
            row_offset += 1;
        }
    };

    int box_y = metrics_start_y;
    for (std::size_t b = 0; b < metric_boxes.size(); ++b) {
        render_box(box_y, metric_boxes[b]);
        box_y += box_rows(metric_boxes[b]);
        if (b + 1 < metric_boxes.size()) box_y += box_gap_rows;
    }

    if (help_lines >= 4) {
        attron(A_DIM);
        mvprintw(LINES - 2, 0, "x: delete   e: edit   c: copy");
        mvprintw(LINES - 1, 0, "h: home   ?: help   s: settings   q: quit");
        attroff(A_DIM);
    }
    else if (help_lines >= 2) {
        attron(A_DIM);
        mvprintw(LINES - 2, 0, "x: delete   e: edit   c: copy");
        mvprintw(LINES - 1, 0, "h: home   ?: help   s: settings   q: quit");
        attroff(A_DIM);
    }

    const int max_cursor_x = std::max(0, COLS - 1);
    move(cursor_y, std::min(cursor_x, max_cursor_x));
    wnoutrefresh(stdscr);
    doupdate();
}

inline bool handle_key_ticker(AppState& app, int ch)
{
    auto& view = app.ticker_view;
    if (view.input_index < 0) view.input_index = 0;
    if (view.input_index > 1) view.input_index = 1;

    if (ch == KEY_UP) {
        if (view.input_index > 0) view.input_index -= 1;
        return true;
    }

    if (ch == KEY_DOWN) {
        if (view.input_index < 1) view.input_index += 1;
        return true;
    }

    if (ch == KEY_LEFT) {
        if (view.index > 0) {
            view.index -= 1;
            view.scroll = 0;
        }
        return true;
    }

    if (ch == KEY_RIGHT) {
        if (view.index + 1 < static_cast<int>(view.rows.size())) {
            view.index += 1;
            view.scroll = 0;
        }
        return true;
    }

    if (ch == KEY_NPAGE
#ifdef KEY_SF
        || ch == KEY_SF
#endif
    ) {
        view.scroll += 3;
        return true;
    }

    if (ch == KEY_PPAGE
#ifdef KEY_SR
        || ch == KEY_SR
#endif
    ) {
        view.scroll -= 3;
        return true;
    }

    if (ch == 'y' || ch == 'Y') {
        if (!view.yearly_only) {
            std::vector<db::Database::FinanceRow> yearly;
            yearly.reserve(view.all_rows.size());
            std::copy_if(view.all_rows.begin(),
                         view.all_rows.end(),
                         std::back_inserter(yearly),
                         is_yearly_period);
            if (yearly.empty()) return true;

            std::string current_period =
                view.rows.empty() ? std::string{}
                                  : period_label(view.rows[view.index]);
            view.rows = std::move(yearly);
            view.yearly_only = true;
            int idx = current_period.empty()
                          ? -1
                          : find_period_index(view.rows, current_period);
            view.index =
                (idx >= 0) ? idx : static_cast<int>(view.rows.size() - 1);
            view.scroll = 0;
            return true;
        }

        std::string current_period = view.rows.empty()
                                         ? std::string{}
                                         : period_label(view.rows[view.index]);
        view.rows = view.all_rows;
        view.yearly_only = false;
        int idx = current_period.empty()
                      ? -1
                      : find_period_index(view.rows, current_period);
        view.index = (idx >= 0) ? idx : static_cast<int>(view.rows.size() - 1);
        view.scroll = 0;
        return true;
    }

    if (ch == 'e' || ch == 'E') {
        if (view.rows.empty()) return true;
        open_add_prefilled_from_ticker(app, view.rows[view.index]);
        return true;
    }

    if (ch == 'c' || ch == 'C') {
        if (view.rows.empty()) return true;

        const std::string text =
            period_clipboard_text(app, view, view.rows[view.index]);
        std::string used;
        if (copy_text_to_clipboard(text, &used)) {
            view.status_line = "copied data to clipboard (" + used + ")";
        }
        else {
            view.status_line = clipboard_unavailable_hint();
        }
        return true;
    }

    const int BACKSPACE_1 = KEY_BACKSPACE;
    const int BACKSPACE_2 = 127;
    const int BACKSPACE_3 = 8;
    if (ch == BACKSPACE_1 || ch == BACKSPACE_2 || ch == BACKSPACE_3) {
        std::string& target = view.inputs[view.input_index];
        if (!target.empty()) target.pop_back();
        return true;
    }

    if (ch == 'x' || ch == 'X') {
        if (view.rows.empty()) return true;

        const std::string current_period = period_label(view.rows[view.index]);
        const int previous_index = view.index - 1;

        std::string err;
        if (!app.db->delete_period(view.ticker, current_period, &err)) {
            route_error(app, err);
            return true;
        }

        app.tickers.invalidate_prefetch();

        auto refreshed = app.db->get_finances(view.ticker, &err);
        if (!err.empty()) {
            route_error(app, err);
            return true;
        }

        if (refreshed.empty()) {
            app.current = views::ViewId::Home;
            return true;
        }

        view.all_rows = std::move(refreshed);

        if (view.yearly_only) {
            std::vector<db::Database::FinanceRow> yearly;
            yearly.reserve(view.all_rows.size());
            std::copy_if(view.all_rows.begin(),
                         view.all_rows.end(),
                         std::back_inserter(yearly),
                         is_yearly_period);
            if (yearly.empty()) {
                view.rows = view.all_rows;
                view.yearly_only = false;
            }
            else {
                view.rows = std::move(yearly);
            }
        }
        else {
            view.rows = view.all_rows;
        }

        view.index = previous_index;
        view.clamp_index();
        view.scroll = 0;
        return true;
    }

    if (ch == KEY_DC) {
        view.inputs[view.input_index].clear();
        return true;
    }

    if (is_allowed_ticker_input_char(ch, view.inputs[view.input_index])) {
        view.inputs[view.input_index].push_back(static_cast<char>(ch));
        return true;
    }

    if (ch == 27 /*ESC*/) {
        app.current = views::ViewId::Home;
        return true;
    }

    return false;
}

} // namespace views
