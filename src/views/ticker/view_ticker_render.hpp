#pragma once

#include "views/ticker/view_ticker_metrics.hpp"

namespace views {
inline void render_ticker_type3(AppState& app,
                                int help_lines,
                                const db::Database::FinanceRow& row,
                                const db::Database::FinanceRow* previous_row,
                                const std::string& period)
{
    auto& view = app.ticker_view;

    if (LINES > 1) {
        mvprintw(1,
                 0,
                 "period: %s (%d/%d)  view: %s",
                 period.c_str(),
                 view.index + 1,
                 static_cast<int>(view.rows.size()),
                 view.yearly_only ? "yearly" : "all");
    }

    const auto net_income_d = to_f64(row.net_income);
    const auto eps_d_current = row.eps;
    const auto total_assets_d = to_f64(row.total_assets);
    const auto total_liabilities_d = to_f64(row.total_liabilities);
    const auto reserves_d = to_f64(row.insurance_reserves);
    const auto earned_premiums_d = to_f64(row.earned_premiums);
    const auto claims_incurred_d = to_f64(row.claims_incurred);
    const auto interest_expenses_d = to_f64(row.interest_expenses);
    const auto total_expenses_d = to_f64(row.total_expenses);
    const auto underwriting_expenses =
        derived_underwriting_expenses_for_row(row);
    const auto underwriting_expenses_d = to_f64(underwriting_expenses);
    const auto total_debt_d = to_f64(row.total_debt);

    const auto equity = sub_i64(row.total_assets, row.total_liabilities);
    const auto equity_d = to_f64(equity);
    const auto underwriting_profit =
        sub_i64(sub_i64(row.earned_premiums, row.claims_incurred),
                underwriting_expenses);
    const auto underwriting_profit_d = to_f64(underwriting_profit);

    const auto prev_net_income_d =
        previous_row ? to_f64(previous_row->net_income) : std::nullopt;
    const auto prev_eps_d = previous_row ? previous_row->eps : std::nullopt;
    const auto prev_total_assets_d =
        previous_row ? to_f64(previous_row->total_assets) : std::nullopt;
    const auto prev_total_liabilities_d =
        previous_row ? to_f64(previous_row->total_liabilities) : std::nullopt;
    const auto prev_reserves_d =
        previous_row ? to_f64(previous_row->insurance_reserves) : std::nullopt;
    const auto prev_earned_premiums_d =
        previous_row ? to_f64(previous_row->earned_premiums) : std::nullopt;
    const auto prev_claims_incurred_d =
        previous_row ? to_f64(previous_row->claims_incurred) : std::nullopt;
    const auto prev_interest_expenses_d =
        previous_row ? to_f64(previous_row->interest_expenses) : std::nullopt;
    const auto prev_total_expenses_d =
        previous_row ? to_f64(previous_row->total_expenses) : std::nullopt;
    const auto prev_underwriting_expenses =
        previous_row ? derived_underwriting_expenses_for_row(*previous_row)
                     : std::nullopt;
    const auto prev_underwriting_expenses_d =
        to_f64(prev_underwriting_expenses);
    const auto prev_total_debt_d =
        previous_row ? to_f64(previous_row->total_debt) : std::nullopt;

    const auto prev_equity = previous_row
                                 ? sub_i64(previous_row->total_assets,
                                           previous_row->total_liabilities)
                                 : std::nullopt;
    const auto prev_equity_d = to_f64(prev_equity);
    const auto prev_underwriting_profit =
        previous_row ? sub_i64(sub_i64(previous_row->earned_premiums,
                                       previous_row->claims_incurred),
                               prev_underwriting_expenses)
                     : std::nullopt;
    const auto prev_underwriting_profit_d = to_f64(prev_underwriting_profit);

    const char family = period_family(row);
    const int ttm_window = ttm_window_for_family(family);
    const bool ttm_family_supported = ttm_window > 0;

    std::optional<double> ttm_eps;
    std::optional<double> ttm_net_income_d;
    if (ttm_family_supported) {
        const int all_index =
            find_period_index(view.all_rows, period_label(row));
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
    }

    const bool prefer_ttm = app.settings.ttm && ttm_family_supported;
    const auto eps_for_derived =
        (prefer_ttm && is_valid_number(ttm_eps)) ? ttm_eps : eps_d_current;
    const auto net_income_for_derived =
        (prefer_ttm && is_valid_number(ttm_net_income_d)) ? ttm_net_income_d
                                                          : net_income_d;
    const auto eps_for_wished = (prefer_ttm && ttm_eps.has_value() &&
                                 std::isfinite(*ttm_eps) && *ttm_eps > 0.0)
                                    ? ttm_eps
                                    : eps_d_current;
    const auto net_income_for_wished =
        (prefer_ttm && ttm_net_income_d.has_value() &&
         std::isfinite(*ttm_net_income_d) && *ttm_net_income_d > 0.0)
            ? ttm_net_income_d
            : net_income_d;

    const auto shares_outstanding_raw =
        div_opt_nonzero(net_income_for_derived, eps_for_derived);
    const auto shares_outstanding =
        shares_outstanding_raw.has_value()
            ? std::optional<double>(std::round(*shares_outstanding_raw))
            : std::nullopt;
    const auto book_value_per_share =
        div_opt_nonzero(equity_d, shares_outstanding);

    const auto prev_shares_outstanding_raw =
        div_opt_nonzero(prev_net_income_d, prev_eps_d);
    const auto prev_shares_outstanding =
        prev_shares_outstanding_raw.has_value()
            ? std::optional<double>(std::round(*prev_shares_outstanding_raw))
            : std::nullopt;
    const auto prev_book_value_per_share =
        div_opt_nonzero(prev_equity_d, prev_shares_outstanding);

    const auto loss_ratio =
        div_opt_nonzero(claims_incurred_d, earned_premiums_d);
    const auto expense_ratio =
        div_opt_nonzero(underwriting_expenses_d, earned_premiums_d);
    const auto combined_ratio =
        (loss_ratio.has_value() && expense_ratio.has_value())
            ? std::optional<double>(*loss_ratio + *expense_ratio)
            : std::nullopt;
    const auto underwriting_margin =
        div_opt_nonzero(underwriting_profit_d, earned_premiums_d);
    const auto roe = div_opt_nonzero(net_income_d, equity_d);
    const auto reserves_to_equity = div_opt_nonzero(reserves_d, equity_d);
    const auto debt_to_equity = div_opt_nonzero(total_debt_d, equity_d);

    const auto prev_loss_ratio =
        div_opt_nonzero(prev_claims_incurred_d, prev_earned_premiums_d);
    const auto prev_expense_ratio =
        div_opt_nonzero(prev_underwriting_expenses_d, prev_earned_premiums_d);
    const auto prev_combined_ratio =
        (prev_loss_ratio.has_value() && prev_expense_ratio.has_value())
            ? std::optional<double>(*prev_loss_ratio + *prev_expense_ratio)
            : std::nullopt;
    const auto prev_underwriting_margin =
        div_opt_nonzero(prev_underwriting_profit_d, prev_earned_premiums_d);
    const auto prev_roe = div_opt_nonzero(prev_net_income_d, prev_equity_d);
    const auto prev_reserves_to_equity =
        div_opt_nonzero(prev_reserves_d, prev_equity_d);
    const auto prev_debt_to_equity =
        div_opt_nonzero(prev_total_debt_d, prev_equity_d);

    const std::optional<double> typed_price =
        parse_decimal_input(view.inputs[0]);
    const std::optional<double> wished_per =
        parse_decimal_input(view.inputs[1]);
    const auto ratio_price = null_if_zero_or_invalid(typed_price);
    const auto p_bv = div_opt_nonzero(ratio_price, book_value_per_share);
    const auto p_e = div_opt_nonzero(ratio_price, eps_for_derived);
    const auto prev_p_bv =
        div_opt_nonzero(ratio_price, prev_book_value_per_share);
    const auto prev_p_e = div_opt_nonzero(ratio_price, prev_eps_d);

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

    const std::vector<Metric> target_box = {
        {"P needed",
         with_change(format_compact_i64_from_f64_opt(
                         price_needed_for_wished_per, kNaValue),
                     price_needed_change),
         false,
         true},
        {"NI needed",
         with_change(
             format_compact_i64_from_f64_opt(required_net_income, kNaValue),
             required_net_income_change),
         false,
         true},
    };

    const std::vector<Metric> valuation_box = {
        {"P / E",
         with_change(format_ratio_opt(p_e, kNaValue),
                     ratio_percent_change(p_e, prev_p_e)),
         true,
         true},
        {"P / BV",
         with_change(format_ratio_opt(p_bv, kNaValue),
                     ratio_percent_change(p_bv, prev_p_bv)),
         true,
         true},
    };

    const std::vector<Metric> balance_box = {
        {"TA",
         with_change(format_i64_opt(row.total_assets),
                     percent_change(total_assets_d, prev_total_assets_d))},
        {"TL",
         with_change(
             format_i64_opt(row.total_liabilities),
             percent_change(total_liabilities_d, prev_total_liabilities_d))},
        {"Resv.",
         with_change(format_i64_opt(row.insurance_reserves),
                     percent_change(reserves_d, prev_reserves_d))},
        {"Debt",
         with_change(format_i64_opt(row.total_debt),
                     percent_change(total_debt_d, prev_total_debt_d))},
        {"E",
         with_change(format_i64_opt(equity),
                     percent_change(equity_d, prev_equity_d))},
        {"Resv. / E",
         with_change(format_ratio_opt(reserves_to_equity),
                     ratio_percent_change(reserves_to_equity,
                                          prev_reserves_to_equity))},
        {"Debt / E",
         with_change(
             format_ratio_opt(debt_to_equity),
             ratio_percent_change(debt_to_equity, prev_debt_to_equity))},
        {"Shs~",
         with_change(
             format_shares_opt(shares_outstanding),
             percent_change(shares_outstanding, prev_shares_outstanding))},
        {"BV",
         with_change(format_ratio_opt(book_value_per_share, kNaValue),
                     ratio_percent_change(book_value_per_share,
                                          prev_book_value_per_share))},
    };

    const std::vector<Metric> income_box = {
        {"Premiums",
         with_change(
             format_i64_opt(row.earned_premiums),
             percent_change(earned_premiums_d, prev_earned_premiums_d))},
        {"Claims",
         with_change(
             format_i64_opt(row.claims_incurred),
             percent_change(claims_incurred_d, prev_claims_incurred_d))},
        {"Interests",
         with_change(
             format_i64_opt(row.interest_expenses),
             percent_change(interest_expenses_d, prev_interest_expenses_d))},
        {"Expenses",
         with_change(format_i64_opt(row.total_expenses),
                     percent_change(total_expenses_d, prev_total_expenses_d))},
        {"UW exp.",
         with_change(format_i64_opt(underwriting_expenses),
                     percent_change(underwriting_expenses_d,
                                    prev_underwriting_expenses_d))},
        {"UW profit",
         with_change(format_i64_opt(underwriting_profit),
                     percent_change(underwriting_profit_d,
                                    prev_underwriting_profit_d))},
        {"NI",
         with_change(format_i64_opt(row.net_income),
                     percent_change(net_income_d, prev_net_income_d))},
        {"EPS",
         with_change(format_f64_opt(row.eps),
                     percent_change(row.eps, prev_eps_d))},
    };

    const std::vector<Metric> ratios_box = {
        {"Loss%",
         with_change(format_f64_opt(loss_ratio, true),
                     ratio_percent_change(loss_ratio, prev_loss_ratio))},
        {"Exp%",
         with_change(format_f64_opt(expense_ratio, true),
                     ratio_percent_change(expense_ratio, prev_expense_ratio))},
        {"Comb%",
         with_change(
             format_f64_opt(combined_ratio, true),
             ratio_percent_change(combined_ratio, prev_combined_ratio))},
        {"UW margin%",
         with_change(format_f64_opt(underwriting_margin, true),
                     ratio_percent_change(underwriting_margin,
                                          prev_underwriting_margin))},
        {"ROE",
         with_change(format_f64_opt(roe, true),
                     ratio_percent_change(roe, prev_roe))},
    };

    constexpr int body_top = 3;
    const int help_start = std::max(0, LINES - help_lines);
    int body_height = std::max(1, help_start - body_top);

    constexpr std::array<const char*, 2> labels = {"price", "wished per"};
    const int input_x = (COLS >= 28) ? 16 : 10;
    const int input_label_w = std::max(4, input_x - 5);

    constexpr int metric_col_gap = 1;
    constexpr int box_gap_rows = 1;
    constexpr int preferred_col_w = 30;
    constexpr int min_label_w_for_two_col = 11;
    constexpr int min_value_w_for_two_col = 12;
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
    const int label_w = std::clamp(col_w - 15, 6, 13);

    const std::vector<std::vector<Metric>> metric_boxes = {
        target_box,
        valuation_box,
        balance_box,
        income_box,
        ratios_box,
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

    render_ticker_help_rows(help_lines, COLS);

    const int max_cursor_x = std::max(0, COLS - 1);
    const int target_cursor_x = std::min(cursor_x, max_cursor_x);
    render_blinking_input_caret(app, cursor_y, target_cursor_x);
    wnoutrefresh(stdscr);
    doupdate();
}

inline void render_ticker_type2(AppState& app,
                                int help_lines,
                                const db::Database::FinanceRow& row,
                                const db::Database::FinanceRow* previous_row,
                                const std::string& period)
{
    auto& view = app.ticker_view;

    if (LINES > 1) {
        mvprintw(1,
                 0,
                 "period: %s (%d/%d)  view: %s",
                 period.c_str(),
                 view.index + 1,
                 static_cast<int>(view.rows.size()),
                 view.yearly_only ? "yearly" : "all");
    }

    const auto net_income_d = to_f64(row.net_income);
    const auto eps_d_current = row.eps;
    const auto loans_d = to_f64(row.total_loans);
    const auto goodwill_d = to_f64(row.goodwill);
    const auto total_assets_d = to_f64(row.total_assets);
    const auto total_deposits_d = to_f64(row.total_deposits);
    const auto total_liabilities_d = to_f64(row.total_liabilities);
    const auto nii_d = to_f64(row.net_interest_income);
    const auto non_ii_d = to_f64(row.non_interest_income);
    const auto llp_d = to_f64(row.loan_loss_provisions);
    const auto non_ie_d = to_f64(row.non_interest_expense);
    const auto rwa_d = to_f64(row.risk_weighted_assets);
    const auto cet1_d = to_f64(row.common_equity_tier1);
    const auto nco_d = to_f64(row.net_charge_offs);
    const auto npl_d = to_f64(row.non_performing_loans);

    const auto equity = sub_i64(row.total_assets, row.total_liabilities);
    const auto equity_d = to_f64(equity);
    const auto tangible_equity = sub_i64(equity, row.goodwill);
    const auto tangible_equity_d = to_f64(tangible_equity);
    const auto pre_provision_profit =
        sub_i64(add_i64(row.net_interest_income, row.non_interest_income),
                row.non_interest_expense);
    const auto ppop_d = to_f64(pre_provision_profit);

    const auto prev_net_income_d =
        previous_row ? to_f64(previous_row->net_income) : std::nullopt;
    const auto prev_eps_d = previous_row ? previous_row->eps : std::nullopt;
    const auto prev_loans_d =
        previous_row ? to_f64(previous_row->total_loans) : std::nullopt;
    const auto prev_goodwill_d =
        previous_row ? to_f64(previous_row->goodwill) : std::nullopt;
    const auto prev_total_assets_d =
        previous_row ? to_f64(previous_row->total_assets) : std::nullopt;
    const auto prev_total_deposits_d =
        previous_row ? to_f64(previous_row->total_deposits) : std::nullopt;
    const auto prev_total_liabilities_d =
        previous_row ? to_f64(previous_row->total_liabilities) : std::nullopt;
    const auto prev_nii_d =
        previous_row ? to_f64(previous_row->net_interest_income) : std::nullopt;
    const auto prev_non_ii_d =
        previous_row ? to_f64(previous_row->non_interest_income) : std::nullopt;
    const auto prev_llp_d = previous_row
                                ? to_f64(previous_row->loan_loss_provisions)
                                : std::nullopt;
    const auto prev_non_ie_d = previous_row
                                   ? to_f64(previous_row->non_interest_expense)
                                   : std::nullopt;
    const auto prev_rwa_d = previous_row
                                ? to_f64(previous_row->risk_weighted_assets)
                                : std::nullopt;
    const auto prev_cet1_d =
        previous_row ? to_f64(previous_row->common_equity_tier1) : std::nullopt;
    const auto prev_nco_d =
        previous_row ? to_f64(previous_row->net_charge_offs) : std::nullopt;
    const auto prev_npl_d = previous_row
                                ? to_f64(previous_row->non_performing_loans)
                                : std::nullopt;

    const auto prev_equity = previous_row
                                 ? sub_i64(previous_row->total_assets,
                                           previous_row->total_liabilities)
                                 : std::nullopt;
    const auto prev_equity_d = to_f64(prev_equity);
    const auto prev_tangible_equity =
        previous_row ? sub_i64(prev_equity, previous_row->goodwill)
                     : std::nullopt;
    const auto prev_tangible_equity_d = to_f64(prev_tangible_equity);
    const auto prev_ppop =
        previous_row ? sub_i64(add_i64(previous_row->net_interest_income,
                                       previous_row->non_interest_income),
                               previous_row->non_interest_expense)
                     : std::nullopt;
    const auto prev_ppop_d = to_f64(prev_ppop);

    const char family = period_family(row);
    const int ttm_window = ttm_window_for_family(family);
    const bool ttm_family_supported = ttm_window > 0;

    std::optional<double> ttm_eps;
    std::optional<double> ttm_net_income_d;
    if (ttm_family_supported) {
        const int all_index =
            find_period_index(view.all_rows, period_label(row));
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
    }

    const bool prefer_ttm = app.settings.ttm && ttm_family_supported;
    const auto eps_for_derived =
        (prefer_ttm && is_valid_number(ttm_eps)) ? ttm_eps : eps_d_current;
    const auto net_income_for_derived =
        (prefer_ttm && is_valid_number(ttm_net_income_d)) ? ttm_net_income_d
                                                          : net_income_d;
    const auto eps_for_wished = (prefer_ttm && ttm_eps.has_value() &&
                                 std::isfinite(*ttm_eps) && *ttm_eps > 0.0)
                                    ? ttm_eps
                                    : eps_d_current;
    const auto net_income_for_wished =
        (prefer_ttm && ttm_net_income_d.has_value() &&
         std::isfinite(*ttm_net_income_d) && *ttm_net_income_d > 0.0)
            ? ttm_net_income_d
            : net_income_d;

    const auto shares_outstanding_raw =
        div_opt_nonzero(net_income_for_derived, eps_for_derived);
    const auto shares_outstanding =
        shares_outstanding_raw.has_value()
            ? std::optional<double>(std::round(*shares_outstanding_raw))
            : std::nullopt;
    const auto tbv_per_share =
        div_opt_nonzero(tangible_equity_d, shares_outstanding);

    const auto prev_shares_outstanding_raw =
        div_opt_nonzero(prev_net_income_d, prev_eps_d);
    const auto prev_shares_outstanding =
        prev_shares_outstanding_raw.has_value()
            ? std::optional<double>(std::round(*prev_shares_outstanding_raw))
            : std::nullopt;
    const auto prev_tbv_per_share =
        div_opt_nonzero(prev_tangible_equity_d, prev_shares_outstanding);

    const auto roa = div_opt_nonzero(net_income_d, total_assets_d);
    const auto rote = div_opt_nonzero(net_income_d, tangible_equity_d);
    const auto ppop_to_assets = div_opt_nonzero(ppop_d, total_assets_d);
    const auto npl_ratio = div_opt_nonzero(npl_d, loans_d);
    const auto chargeoff_ratio = div_opt_nonzero(nco_d, loans_d);
    const auto provision_ratio = div_opt_nonzero(llp_d, loans_d);
    const auto provision_to_ppop = div_opt_nonzero(llp_d, ppop_d);
    const auto cet1_ratio = div_opt_nonzero(cet1_d, rwa_d);
    const auto leverage = div_opt_nonzero(total_assets_d, tangible_equity_d);
    const auto loan_to_deposit = div_opt_nonzero(loans_d, total_deposits_d);

    const auto prev_roa =
        div_opt_nonzero(prev_net_income_d, prev_total_assets_d);
    const auto prev_rote =
        div_opt_nonzero(prev_net_income_d, prev_tangible_equity_d);
    const auto prev_ppop_to_assets =
        div_opt_nonzero(prev_ppop_d, prev_total_assets_d);
    const auto prev_npl_ratio = div_opt_nonzero(prev_npl_d, prev_loans_d);
    const auto prev_chargeoff_ratio = div_opt_nonzero(prev_nco_d, prev_loans_d);
    const auto prev_provision_ratio = div_opt_nonzero(prev_llp_d, prev_loans_d);
    const auto prev_provision_to_ppop =
        div_opt_nonzero(prev_llp_d, prev_ppop_d);
    const auto prev_cet1_ratio = div_opt_nonzero(prev_cet1_d, prev_rwa_d);
    const auto prev_leverage =
        div_opt_nonzero(prev_total_assets_d, prev_tangible_equity_d);
    const auto prev_loan_to_deposit =
        div_opt_nonzero(prev_loans_d, prev_total_deposits_d);

    const std::optional<double> typed_price =
        parse_decimal_input(view.inputs[0]);
    const std::optional<double> wished_per =
        parse_decimal_input(view.inputs[1]);
    const auto ratio_price = null_if_zero_or_invalid(typed_price);
    const auto p_tbv = div_opt_nonzero(ratio_price, tbv_per_share);
    const auto p_e = div_opt_nonzero(ratio_price, eps_for_derived);
    const auto prev_p_tbv = div_opt_nonzero(ratio_price, prev_tbv_per_share);
    const auto prev_p_e = div_opt_nonzero(ratio_price, prev_eps_d);

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

    const std::vector<Metric> target_box = {
        {"P needed",
         with_change(format_compact_i64_from_f64_opt(
                         price_needed_for_wished_per, kNaValue),
                     price_needed_change),
         false,
         true},
        {"NI needed",
         with_change(
             format_compact_i64_from_f64_opt(required_net_income, kNaValue),
             required_net_income_change),
         false,
         true},
    };

    const std::vector<Metric> valuation_box = {
        {"P / E",
         with_change(format_ratio_opt(p_e, kNaValue),
                     ratio_percent_change(p_e, prev_p_e)),
         true,
         true},
        {"P / TBV",
         with_change(format_ratio_opt(p_tbv, kNaValue),
                     ratio_percent_change(p_tbv, prev_p_tbv)),
         true,
         true},
    };

    const std::vector<Metric> balance_reg_box = {
        {"TA",
         with_change(format_i64_opt(row.total_assets),
                     percent_change(total_assets_d, prev_total_assets_d))},
        {"TL",
         with_change(
             format_i64_opt(row.total_liabilities),
             percent_change(total_liabilities_d, prev_total_liabilities_d))},
        {"Loans",
         with_change(format_i64_opt(row.total_loans),
                     percent_change(loans_d, prev_loans_d))},
        {"Dep.",
         with_change(format_i64_opt(row.total_deposits),
                     percent_change(total_deposits_d, prev_total_deposits_d))},
        {"Goodwill",
         with_change(format_i64_opt(row.goodwill),
                     percent_change(goodwill_d, prev_goodwill_d))},
        {"Loans / Dep.",
         with_change(
             format_f64_opt(loan_to_deposit, true),
             ratio_percent_change(loan_to_deposit, prev_loan_to_deposit))},
        {"E",
         with_change(format_i64_opt(equity),
                     percent_change(equity_d, prev_equity_d))},
        {"TE",
         with_change(
             format_i64_opt(tangible_equity),
             percent_change(tangible_equity_d, prev_tangible_equity_d))},
        {"Lev.",
         with_change(format_ratio_opt(leverage),
                     ratio_percent_change(leverage, prev_leverage))},
        {"Shs~",
         with_change(
             format_shares_opt(shares_outstanding),
             percent_change(shares_outstanding, prev_shares_outstanding))},
        {"TBV",
         with_change(format_ratio_opt(tbv_per_share, kNaValue),
                     ratio_percent_change(tbv_per_share, prev_tbv_per_share))},
    };

    const std::vector<Metric> earnings_box = {
        {"NII",
         with_change(format_i64_opt(row.net_interest_income),
                     percent_change(nii_d, prev_nii_d))},
        {"Non-int. inc.",
         with_change(format_i64_opt(row.non_interest_income),
                     percent_change(non_ii_d, prev_non_ii_d))},
        {"Non-int. exp.",
         with_change(format_i64_opt(row.non_interest_expense),
                     percent_change(non_ie_d, prev_non_ie_d))},
        {"PPOP",
         with_change(format_i64_opt(pre_provision_profit),
                     percent_change(ppop_d, prev_ppop_d))},
        {"LLP",
         with_change(format_i64_opt(row.loan_loss_provisions),
                     percent_change(llp_d, prev_llp_d))},
        {"LLP / PPOP",
         with_change(
             format_f64_opt(provision_to_ppop, true),
             ratio_percent_change(provision_to_ppop, prev_provision_to_ppop))},
        {"NI",
         with_change(format_i64_opt(row.net_income),
                     percent_change(net_income_d, prev_net_income_d))},
        {"EPS",
         with_change(format_f64_opt(row.eps),
                     percent_change(row.eps, prev_eps_d))},
        {"ROA",
         with_change(format_f64_opt(roa, true),
                     ratio_percent_change(roa, prev_roa))},
        {"ROTE",
         with_change(format_f64_opt(rote, true),
                     ratio_percent_change(rote, prev_rote))},
        {"PPOP / A",
         with_change(
             format_f64_opt(ppop_to_assets, true),
             ratio_percent_change(ppop_to_assets, prev_ppop_to_assets))},
    };

    const std::vector<Metric> asset_quality_box = {
        {"RWA",
         with_change(format_i64_opt(row.risk_weighted_assets),
                     percent_change(rwa_d, prev_rwa_d))},
        {"CET1",
         with_change(format_i64_opt(row.common_equity_tier1),
                     percent_change(cet1_d, prev_cet1_d))},
        {"Prov%",
         with_change(
             format_f64_opt(provision_ratio, true),
             ratio_percent_change(provision_ratio, prev_provision_ratio))},
        {"CET1%",
         with_change(format_f64_opt(cet1_ratio, true),
                     ratio_percent_change(cet1_ratio, prev_cet1_ratio))},
        {"NPL",
         with_change(format_i64_opt(row.non_performing_loans),
                     percent_change(npl_d, prev_npl_d))},
        {"NCO",
         with_change(format_i64_opt(row.net_charge_offs),
                     percent_change(nco_d, prev_nco_d))},
        {"NPL%",
         with_change(format_f64_opt(npl_ratio, true),
                     ratio_percent_change(npl_ratio, prev_npl_ratio))},
        {"NCO%",
         with_change(
             format_f64_opt(chargeoff_ratio, true),
             ratio_percent_change(chargeoff_ratio, prev_chargeoff_ratio))},
    };

    constexpr int body_top = 3;
    const int help_start = std::max(0, LINES - help_lines);
    int body_height = std::max(1, help_start - body_top);

    constexpr std::array<const char*, 2> labels = {"price", "wished per"};
    const int input_x = (COLS >= 28) ? 16 : 10;
    const int input_label_w = std::max(4, input_x - 5);

    constexpr int metric_col_gap = 1;
    constexpr int box_gap_rows = 1;
    constexpr int preferred_col_w = 30;
    constexpr int min_label_w_for_two_col = 11;
    constexpr int min_value_w_for_two_col = 12;
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
    const int label_w = std::clamp(col_w - 15, 6, 13);

    const std::vector<std::vector<Metric>> metric_boxes = {
        target_box,
        valuation_box,
        balance_reg_box,
        earnings_box,
        asset_quality_box,
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

    render_ticker_help_rows(help_lines, COLS);

    const int max_cursor_x = std::max(0, COLS - 1);
    const int target_cursor_x = std::min(cursor_x, max_cursor_x);
    render_blinking_input_caret(app, cursor_y, target_cursor_x);
    wnoutrefresh(stdscr);
    doupdate();
}

} // namespace views


