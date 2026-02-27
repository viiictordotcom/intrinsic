#pragma once
#include <curses.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "state.hpp"
#include "views/view_add.hpp"

namespace views {

struct Metric {
    const char* label;
    std::string value;
    bool invert_change_color = false;
    bool input_dependent = false;
};

inline constexpr const char* kNaValue = "--";
inline constexpr std::size_t kTickerInputMaxLen = 16;
inline constexpr short kColorPairPositive = 1;
inline constexpr short kColorPairNegative = 2;
inline constexpr short kColorPairHeader = 3;
inline constexpr short kColorPairInputValue = 4;

inline std::string format_f64_raw(double v)
{
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2) << v;
    const std::string raw = oss.str();
    const std::size_t dot = raw.find('.');
    if (dot == std::string::npos) return raw;

    const auto group_int_text = [](const std::string& int_text) {
        if (int_text.empty()) return int_text;

        const bool neg = int_text[0] == '-';
        const std::size_t start = neg ? 1U : 0U;
        const std::size_t digits = int_text.size() - start;
        if (digits <= 3) return int_text;

        std::string out;
        out.reserve(int_text.size() + ((digits - 1) / 3));
        if (neg) out.push_back('-');

        std::size_t pos = start;
        std::size_t head = digits % 3;
        if (head == 0) head = 3;
        out.append(int_text, pos, head);
        pos += head;

        while (pos < int_text.size()) {
            out.push_back(',');
            out.append(int_text, pos, 3);
            pos += 3;
        }
        return out;
    };

    const std::string grouped_int = group_int_text(raw.substr(0, dot));
    return grouped_int + raw.substr(dot);
}

inline std::string format_f64_opt(std::optional<double> v,
                                  bool percent = false,
                                  const char* na_value = kNaValue)
{
    if (!v.has_value()) return na_value;
    double shown = *v;
    if (percent) shown *= 100.0;
    std::string out = format_f64_raw(shown);
    if (percent) out += "%";
    return out;
}

inline std::string format_ratio_opt(std::optional<double> v,
                                    const char* na_value = kNaValue)
{
    return format_f64_opt(v, false, na_value);
}

inline std::string format_clip_f64_value(double v)
{
    std::ostringstream oss;
    oss << std::setprecision(12) << v;
    return oss.str();
}

inline bool pipe_text_to_command(const char* command, const std::string& text)
{
    FILE* pipe = popen(command, "w");
    if (!pipe) return false;

    const std::size_t written = std::fwrite(text.data(), 1, text.size(), pipe);
    const int rc = pclose(pipe);
    return written == text.size() && rc == 0;
}

inline bool copy_text_to_clipboard(const std::string& text, std::string* used)
{
    struct CopyCandidate {
        const char* name;
        const char* command;
    };

#if defined(__APPLE__)
    static constexpr std::array<CopyCandidate, 1> candidates = {{
        {"pbcopy", "pbcopy 2>/dev/null"},
    }};
#else
    static constexpr std::array<CopyCandidate, 3> candidates = {{
        {"wl-copy", "wl-copy 2>/dev/null"},
        {"xclip", "xclip -selection clipboard 2>/dev/null"},
        {"xsel", "xsel --clipboard --input 2>/dev/null"},
    }};
#endif

    const auto it =
        std::find_if(candidates.begin(),
                     candidates.end(),
                     [&](const CopyCandidate& candidate) {
                         return pipe_text_to_command(candidate.command, text);
                     });
    if (it != candidates.end()) {
        if (used) *used = it->name;
        return true;
    }

    if (used) used->clear();
    return false;
}

inline const char* clipboard_unavailable_hint()
{
#if defined(__APPLE__)
    return "clipboard unavailable (expected pbcopy)";
#else
    return "clipboard unavailable (expected wl-copy/xclip/xsel)";
#endif
}

inline std::string format_i64_value(std::int64_t v)
{
    constexpr std::int64_t kOneThousand = 1000;
    constexpr std::int64_t kOneMillion = 1000000;
    constexpr std::int64_t kOneTrillion = 1000000000000;

    const auto group_i64 = [](std::int64_t value) {
        std::string raw = std::to_string(value);
        const bool neg = !raw.empty() && raw[0] == '-';
        const std::size_t start = neg ? 1U : 0U;
        const std::size_t digits = raw.size() - start;
        if (digits <= 3) return raw;

        std::string out;
        out.reserve(raw.size() + ((digits - 1) / 3));
        if (neg) out.push_back('-');

        std::size_t pos = start;
        std::size_t head = digits % 3;
        if (head == 0) head = 3;
        out.append(raw, pos, head);
        pos += head;

        while (pos < raw.size()) {
            out.push_back(',');
            out.append(raw, pos, 3);
            pos += 3;
        }
        return out;
    };

    if (v >= kOneTrillion || v <= -kOneTrillion) {
        return group_i64(v / kOneTrillion) + "T";
    }
    if (v >= kOneMillion || v <= -kOneMillion) {
        return group_i64(v / kOneMillion) + "M";
    }
    if (v >= kOneThousand || v <= -kOneThousand) {
        return group_i64(v / kOneThousand) + "K";
    }
    return group_i64(v);
}

inline std::string format_i64_opt(std::optional<std::int64_t> v)
{
    if (!v.has_value()) return kNaValue;
    return format_i64_value(*v);
}

inline std::string
format_compact_i64_from_f64_opt(std::optional<double> v,
                                const char* na_value = kNaValue)
{
    if (!v.has_value() || !std::isfinite(*v)) return na_value;

    constexpr double kI64Min =
        static_cast<double>(std::numeric_limits<std::int64_t>::min());
    constexpr double kI64Max =
        static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (*v < kI64Min || *v > kI64Max) return na_value;

    const auto rounded = static_cast<std::int64_t>(std::llround(*v));
    return format_i64_value(rounded);
}

inline std::string format_f64_integer_opt(std::optional<double> v,
                                          const char* na_value = kNaValue)
{
    if (!v.has_value() || !std::isfinite(*v)) return na_value;

    constexpr double kI64Min =
        static_cast<double>(std::numeric_limits<std::int64_t>::min());
    constexpr double kI64Max =
        static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (*v < kI64Min || *v > kI64Max) return na_value;

    const auto rounded = static_cast<std::int64_t>(std::llround(*v));
    std::string raw = std::to_string(rounded);
    const bool neg = !raw.empty() && raw[0] == '-';
    const std::size_t start = neg ? 1U : 0U;
    const std::size_t digits = raw.size() - start;
    if (digits <= 3) return raw;

    std::string out;
    out.reserve(raw.size() + ((digits - 1) / 3));
    if (neg) out.push_back('-');

    std::size_t pos = start;
    std::size_t head = digits % 3;
    if (head == 0) head = 3;
    out.append(raw, pos, head);
    pos += head;

    while (pos < raw.size()) {
        out.push_back(',');
        out.append(raw, pos, 3);
        pos += 3;
    }
    return out;
}

inline std::optional<std::int64_t> add_i64(std::optional<std::int64_t> a,
                                           std::optional<std::int64_t> b)
{
    if (!a.has_value() || !b.has_value()) return std::nullopt;
    return *a + *b;
}

inline std::optional<std::int64_t> sub_i64(std::optional<std::int64_t> a,
                                           std::optional<std::int64_t> b)
{
    if (!a.has_value() || !b.has_value()) return std::nullopt;
    return *a - *b;
}

inline std::optional<double> to_f64(std::optional<std::int64_t> v)
{
    if (!v.has_value()) return std::nullopt;
    return static_cast<double>(*v);
}

inline std::optional<double> div_opt(std::optional<double> num,
                                     std::optional<double> den)
{
    if (!num.has_value() || !den.has_value()) return std::nullopt;
    if (*den == 0.0) return std::nullopt;
    return *num / *den;
}

inline bool has_non_zero_value(std::optional<double> v)
{
    return v.has_value() && std::isfinite(*v) && *v != 0.0;
}

inline std::optional<double> null_if_zero_or_invalid(std::optional<double> v)
{
    if (!has_non_zero_value(v)) return std::nullopt;
    return v;
}

inline std::optional<double> null_if_negative(std::optional<double> v)
{
    if (!v.has_value() || !std::isfinite(*v)) return std::nullopt;
    if (*v < 0.0) return std::nullopt;
    return v;
}

inline std::optional<double> div_opt_nonzero(std::optional<double> num,
                                             std::optional<double> den)
{
    if (!has_non_zero_value(num) || !has_non_zero_value(den))
        return std::nullopt;
    return *num / *den;
}

inline std::optional<double> mul_opt_nonzero(std::optional<double> a,
                                             std::optional<double> b)
{
    if (!has_non_zero_value(a) || !has_non_zero_value(b)) return std::nullopt;
    return *a * *b;
}

inline std::optional<double> mul_opt(std::optional<double> a,
                                     std::optional<double> b)
{
    if (!a.has_value() || !b.has_value()) return std::nullopt;
    return *a * *b;
}

inline std::optional<double> parse_decimal_input(const std::string& text)
{
    if (text.empty() || text == ".") return std::nullopt;
    try {
        std::size_t idx = 0;
        const double v = std::stod(text, &idx);
        if (idx != text.size()) return std::nullopt;
        return v;
    }
    catch (...) {
        return std::nullopt;
    }
}

inline bool is_allowed_ticker_input_char(int ch, const std::string& buf)
{
    if (ch < 0 || ch > 255) return false;
    if (buf.size() >= kTickerInputMaxLen) return false;
    const unsigned char c = static_cast<unsigned char>(ch);
    if (std::isdigit(c)) return true;
    if (c == '.' && buf.find('.') == std::string::npos) return true;
    return false;
}

inline bool is_valid_number(std::optional<double> v)
{
    return v.has_value() && std::isfinite(*v);
}

inline char period_family(const db::Database::FinanceRow& row)
{
    if (row.period_type.empty()) return '\0';
    return static_cast<char>(
        std::toupper(static_cast<unsigned char>(row.period_type[0])));
}

inline int ttm_window_for_family(char family)
{
    if (family == 'Q') return 4;
    if (family == 'S') return 2;
    return 0;
}

template <class Getter>
inline std::optional<double>
ttm_sum_for_family(const std::vector<db::Database::FinanceRow>& rows,
                   int from_index,
                   char family,
                   int required_periods,
                   Getter getter)
{
    if (from_index < 0 || required_periods <= 0) return std::nullopt;

    int collected = 0;
    double sum = 0.0;

    for (int i = from_index; i >= 0 && collected < required_periods; --i) {
        if (period_family(rows[i]) != family) continue;
        auto value = getter(rows[i]);
        if (!value.has_value() || !std::isfinite(*value)) return std::nullopt;
        sum += *value;
        collected += 1;
    }

    if (collected < required_periods) return std::nullopt;
    return sum;
}

inline std::string period_label(const db::Database::FinanceRow& row)
{
    return std::to_string(row.year) + "-" + row.period_type;
}

inline bool is_yearly_period(const db::Database::FinanceRow& row)
{
    return row.period_type == "Y";
}

inline int find_period_index(const std::vector<db::Database::FinanceRow>& rows,
                             const std::string& period)
{
    const auto it = std::find_if(rows.begin(), rows.end(), [&](const auto& r) {
        return period_label(r) == period;
    });
    if (it == rows.end()) return -1;
    return static_cast<int>(std::distance(rows.begin(), it));
}

inline const db::Database::FinanceRow* find_previous_year_same_period(
    const std::vector<db::Database::FinanceRow>& rows,
    const db::Database::FinanceRow& row)
{
    const int prev_year = row.year - 1;
    const auto it =
        std::find_if(rows.begin(), rows.end(), [&](const auto& candidate) {
            return candidate.year == prev_year &&
                   candidate.period_type == row.period_type;
        });
    if (it == rows.end()) return nullptr;
    return &(*it);
}

inline void append_clipboard_i64(std::ostringstream& out,
                                 const char* label,
                                 std::optional<std::int64_t> value)
{
    if (!value.has_value()) return;
    out << label << ": " << *value << "\n";
}

inline void append_clipboard_f64(std::ostringstream& out,
                                 const char* label,
                                 std::optional<double> value)
{
    if (!value.has_value()) return;
    if (!std::isfinite(*value)) return;
    out << label << ": " << format_clip_f64_value(*value) << "\n";
}

inline std::string period_clipboard_text(const AppState& app,
                                         const AppState::TickerViewState& view,
                                         const db::Database::FinanceRow& row)
{
    std::ostringstream out;
    out << "period: " << row.year << "-" << row.period_type << "\n";
    if (view.ticker_type == 2) {
        append_clipboard_i64(out, "total loans", row.total_loans);
        append_clipboard_i64(out, "goodwill", row.goodwill);
        append_clipboard_i64(out, "total assets", row.total_assets);
        append_clipboard_i64(out, "total deposits", row.total_deposits);
        append_clipboard_i64(out, "total liabilities", row.total_liabilities);
        append_clipboard_i64(out, "net interest income", row.net_interest_income);
        append_clipboard_i64(out, "non-interest income", row.non_interest_income);
        append_clipboard_i64(
            out, "loan loss provisions", row.loan_loss_provisions);
        append_clipboard_i64(
            out, "non-interest expense", row.non_interest_expense);
        append_clipboard_i64(out, "net income", row.net_income);
        append_clipboard_f64(out, "eps", row.eps);
        append_clipboard_i64(out, "risk-weighted assets", row.risk_weighted_assets);
        append_clipboard_i64(out, "common equity tier1", row.common_equity_tier1);
        append_clipboard_i64(out, "net charge-offs", row.net_charge_offs);
        append_clipboard_i64(out, "non-performing loans", row.non_performing_loans);

        const auto equity = sub_i64(row.total_assets, row.total_liabilities);
        const auto tangible_equity = sub_i64(equity, row.goodwill);
        const auto pre_provision_profit =
            sub_i64(add_i64(row.net_interest_income, row.non_interest_income),
                    row.non_interest_expense);
        const auto net_income_d = to_f64(row.net_income);
        const auto eps_d_current = row.eps;
        const auto assets_d = to_f64(row.total_assets);
        const auto loans_d = to_f64(row.total_loans);
        const auto deposits_d = to_f64(row.total_deposits);
        const auto tangible_equity_d = to_f64(tangible_equity);
        const auto ppop_d = to_f64(pre_provision_profit);
        const auto llp_d = to_f64(row.loan_loss_provisions);
        const auto nco_d = to_f64(row.net_charge_offs);
        const auto npl_d = to_f64(row.non_performing_loans);
        const auto rwa_d = to_f64(row.risk_weighted_assets);
        const auto cet1_d = to_f64(row.common_equity_tier1);
        const char family = period_family(row);
        const int ttm_window = ttm_window_for_family(family);
        const bool ttm_family_supported = ttm_window > 0;

        std::optional<double> ttm_eps;
        std::optional<double> ttm_net_income_d;

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
        }

        const bool prefer_ttm_for_derived =
            app.settings.ttm && ttm_family_supported;
        const auto eps_for_derived =
            (prefer_ttm_for_derived && is_valid_number(ttm_eps)) ? ttm_eps
                                                                 : eps_d_current;
        const auto net_income_for_derived =
            (prefer_ttm_for_derived && is_valid_number(ttm_net_income_d))
                ? ttm_net_income_d
                : net_income_d;

        const auto shares_outstanding_raw =
            div_opt_nonzero(net_income_for_derived, eps_for_derived);
        const auto shares_outstanding =
            shares_outstanding_raw.has_value()
                ? std::optional<double>(std::round(*shares_outstanding_raw))
                : std::nullopt;
        const auto tbv_per_share = div_opt_nonzero(tangible_equity_d, shares_outstanding);
        const std::optional<double> typed_price = parse_decimal_input(view.inputs[0]);
        const auto ratio_price = null_if_zero_or_invalid(typed_price);
        const auto p_tbv = div_opt_nonzero(ratio_price, tbv_per_share);
        const auto p_e = div_opt_nonzero(ratio_price, eps_for_derived);

        append_clipboard_i64(out, "equity", equity);
        append_clipboard_i64(out, "tangible equity", tangible_equity);
        append_clipboard_i64(out, "pre-provision profit", pre_provision_profit);
        append_clipboard_f64(out, "shares approx", shares_outstanding);
        append_clipboard_f64(out, "tbv per share", tbv_per_share);
        append_clipboard_f64(out, "roa", div_opt_nonzero(net_income_d, assets_d));
        append_clipboard_f64(out,
                            "rote",
                            div_opt_nonzero(net_income_d, tangible_equity_d));
        append_clipboard_f64(out,
                            "ppop / assets",
                            div_opt_nonzero(ppop_d, assets_d));
        append_clipboard_f64(out, "npl ratio", div_opt_nonzero(npl_d, loans_d));
        append_clipboard_f64(
            out, "chargeoff ratio", div_opt_nonzero(nco_d, loans_d));
        append_clipboard_f64(
            out, "provision ratio", div_opt_nonzero(llp_d, loans_d));
        append_clipboard_f64(
            out, "provision / ppop", div_opt_nonzero(llp_d, ppop_d));
        append_clipboard_f64(out, "cet1 ratio", div_opt_nonzero(cet1_d, rwa_d));
        append_clipboard_f64(
            out, "leverage", div_opt_nonzero(assets_d, tangible_equity_d));
        append_clipboard_f64(
            out, "loan / deposit", div_opt_nonzero(loans_d, deposits_d));
        append_clipboard_f64(out, "p / tbv", p_tbv);
        append_clipboard_f64(out, "p / e", p_e);
        return out.str();
    }

    append_clipboard_i64(out, "cash and equivalents", row.cash_and_equivalents);
    append_clipboard_i64(out, "current assets", row.current_assets);
    append_clipboard_i64(out, "non-current assets", row.non_current_assets);
    append_clipboard_i64(out, "current liabilities", row.current_liabilities);
    append_clipboard_i64(
        out, "non-current liabilities", row.non_current_liabilities);
    append_clipboard_i64(out, "revenue", row.revenue);
    append_clipboard_i64(out, "net income", row.net_income);
    append_clipboard_f64(out, "eps", row.eps);
    append_clipboard_i64(
        out, "cash flow operations", row.cash_flow_from_operations);
    append_clipboard_i64(
        out, "cash flow investing", row.cash_flow_from_investing);
    append_clipboard_i64(
        out, "cash flow financing", row.cash_flow_from_financing);

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
    const auto current_liabilities_d = to_f64(row.current_liabilities);
    const auto non_current_liabilities_d = to_f64(row.non_current_liabilities);
    const auto cash_d = to_f64(row.cash_and_equivalents);
    const auto cash_flow_ops_d_current = to_f64(row.cash_flow_from_operations);
    const auto eps_d_current = row.eps;

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

    const auto net_margin = div_opt_nonzero(net_income_d, revenue_d);
    const auto roa = div_opt_nonzero(net_income_d, total_assets_d);
    const auto roe = div_opt_nonzero(net_income_d, equity_d);
    const auto liquidity =
        div_opt_nonzero(current_assets_d, current_liabilities_d);
    const auto solvency = div_opt_nonzero(total_assets_d, total_liabilities_d);
    const auto leverage = div_opt_nonzero(total_liabilities_d, equity_d);
    const auto wc_over_non_current =
        div_opt_nonzero(to_f64(working_capital), non_current_liabilities_d);
    const auto shares_approx_raw =
        div_opt_nonzero(net_income_for_derived, eps_for_derived);
    const auto shares_approx =
        shares_approx_raw.has_value()
            ? std::optional<double>(std::round(*shares_approx_raw))
            : std::nullopt;
    const auto book_value = div_opt_nonzero(equity_d, shares_approx);

    const std::optional<double> typed_price =
        parse_decimal_input(view.inputs[0]);
    const auto ratio_price = null_if_zero_or_invalid(typed_price);
    const auto ratio_total_liabilities =
        null_if_zero_or_invalid(total_liabilities_d);
    const auto ratio_cash = null_if_zero_or_invalid(cash_d);

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

    append_clipboard_i64(out, "total assets", total_assets);
    append_clipboard_i64(out, "total liabilities", total_liabilities);
    append_clipboard_i64(out, "equity", equity);
    append_clipboard_i64(out, "working capital", working_capital);
    append_clipboard_f64(out, "wc / non-current liab", wc_over_non_current);
    append_clipboard_f64(out, "shares approx", shares_approx);
    append_clipboard_f64(out, "book value", book_value);
    append_clipboard_f64(out, "net margin", net_margin);
    append_clipboard_f64(out, "roa", roa);
    append_clipboard_f64(out, "roe", roe);
    append_clipboard_f64(out, "liquidity", liquidity);
    append_clipboard_f64(out, "solvency", solvency);
    append_clipboard_f64(out, "leverage", leverage);
    append_clipboard_f64(out, "market cap", market_cap);
    append_clipboard_f64(out, "enterprise value", enterprise_value);
    append_clipboard_f64(out, "ev / cash flow ops", ev_over_cash_flow_ops);
    append_clipboard_f64(out, "per", per_ratio);
    append_clipboard_f64(out, "price / book value", price_to_book);
    append_clipboard_f64(out, "ev / market cap", ev_over_market_cap);
    append_clipboard_f64(out, "ev / net income", ev_over_net_income);

    return out.str();
}

inline std::optional<double> percent_change(std::optional<double> current,
                                            std::optional<double> previous)
{
    if (!current.has_value() || !previous.has_value()) return std::nullopt;
    if (!std::isfinite(*current) || !std::isfinite(*previous))
        return std::nullopt;
    if (*previous == 0.0) return std::nullopt;
    return ((*current - *previous) / std::abs(*previous)) * 100.0;
}

inline std::optional<double>
ratio_percent_change(std::optional<double> current,
                     std::optional<double> previous)
{
    if (!current.has_value() || !previous.has_value()) return std::nullopt;
    if (!std::isfinite(*current) || !std::isfinite(*previous))
        return std::nullopt;
    if (*current == 0.0 || *previous == 0.0) return std::nullopt;

    if (*previous < 0.0 && *current < 0.0) {
        return ((std::abs(*previous) - std::abs(*current)) /
                std::abs(*previous)) *
               100.0;
    }

    if (*previous < 0.0 && *current > 0.0) {
        return std::abs(((*current - *previous) / *previous) * 100.0);
    }

    return ((*current - *previous) / *previous) * 100.0;
}

inline std::optional<double>
required_net_income_change_pct(std::optional<double> required_net_income,
                               std::optional<double> baseline_net_income)
{
    if (!required_net_income.has_value() || !baseline_net_income.has_value())
        return std::nullopt;
    if (!std::isfinite(*required_net_income) ||
        !std::isfinite(*baseline_net_income))
        return std::nullopt;
    if (*baseline_net_income == 0.0) return std::nullopt;

    if (*baseline_net_income < 0.0 && *required_net_income > 0.0) {
        return ((*required_net_income - *baseline_net_income) /
                *required_net_income) *
               100.0;
    }

    return ((*required_net_income - *baseline_net_income) /
            std::abs(*baseline_net_income)) *
           100.0;
}

inline std::optional<double>
rounded_price_for_wished_per(std::optional<double> wished_per,
                             std::optional<double> eps_to_use,
                             std::optional<double> base_eps)
{
    if (!wished_per.has_value() || !eps_to_use.has_value() ||
        !base_eps.has_value())
        return std::nullopt;
    if (!std::isfinite(*wished_per) || !std::isfinite(*eps_to_use) ||
        !std::isfinite(*base_eps))
        return std::nullopt;
    if (*wished_per == 0.0 || *base_eps <= 0.0) return std::nullopt;
    return std::round(*wished_per * *eps_to_use);
}

inline std::string format_change(std::optional<double> change)
{
    if (!change.has_value() || !std::isfinite(*change)) return {};
    const double abs_change = std::abs(*change);
    if (abs_change >= 1000.0) {
        const int k_value = static_cast<int>(std::round(abs_change / 1000.0));
        if (*change < 0.0) return "-" + std::to_string(k_value) + "k%";
        return std::to_string(k_value) + "k%";
    }

    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(1) << *change << "%";
    return oss.str();
}

inline std::string with_change(const std::string& value,
                               std::optional<double> change)
{
    const std::string change_text = format_change(change);
    if (change_text.empty()) return value;
    return value + " " + change_text;
}

inline bool split_value_and_change(const std::string& text,
                                   std::string* value,
                                   std::string* change)
{
    const std::size_t split = text.rfind(' ');
    if (split == std::string::npos) return false;
    if (split + 1 >= text.size()) return false;
    if (text.back() != '%') return false;
    if (value) *value = text.substr(0, split);
    if (change) *change = text.substr(split + 1);
    return true;
}

inline short color_pair_for_change_text(const std::string& change_text,
                                        bool invert = false)
{
    if (change_text.empty()) return 0;

    short pair = 0;
    if (change_text[0] == '-') {
        pair = kColorPairNegative;
    }
    else {
        try {
            const double v = std::stod(change_text);
            if (v > 0.0) pair = kColorPairPositive;
            if (v < 0.0) pair = kColorPairNegative;
        }
        catch (...) {
        }
    }

    if (!invert) return pair;
    if (pair == kColorPairPositive) return kColorPairNegative;
    if (pair == kColorPairNegative) return kColorPairPositive;
    return pair;
}

inline bool is_zero_change_text(const std::string& change_text)
{
    if (change_text.empty()) return false;
    try {
        const double v = std::stod(change_text);
        return std::isfinite(v) && std::abs(v) < 1e-9;
    }
    catch (...) {
        return false;
    }
}

struct LabelDimSpan {
    int start = 0;
    int len = 0;
};

inline std::optional<LabelDimSpan> label_dim_span(std::string_view label)
{
    if (label == "EVcap") return LabelDimSpan{2, 3};
    if (label == "Mnet") return LabelDimSpan{1, 3};
    if (label == "CFop") return LabelDimSpan{2, 2};
    if (label == "CFinv") return LabelDimSpan{2, 3};
    if (label == "CFfin") return LabelDimSpan{2, 3};
    if (label == "EV / CFop") return LabelDimSpan{7, 2};
    return std::nullopt;
}

inline void render_metric_label(int y, int x, int width, const char* label_cstr)
{
    if (width <= 0 || x >= COLS) return;

    std::string label = label_cstr ? label_cstr : "";
    if (static_cast<int>(label.size()) > width) label.resize(width);
    const auto span = label_dim_span(label);

    for (int i = 0; i < width && (x + i) < COLS; ++i) {
        const bool dim = span.has_value() && i >= span->start &&
                         i < (span->start + span->len);
        if (dim) attron(A_DIM);
        mvaddch(y, x + i, i < static_cast<int>(label.size()) ? label[i] : ' ');
        if (dim) attroff(A_DIM);
    }
}

inline void
render_metric_value(int y, int x, int width, const std::string& value)
{
    if (width <= 0 || x >= COLS) return;
    const bool dim_na = value == kNaValue;
    if (dim_na) attron(A_DIM);
    mvprintw(y, x, "%.*s", width, value.c_str());
    if (dim_na) attroff(A_DIM);
}

inline bool input_metric_overflows_width(std::string_view value_text,
                                         std::string_view change_text,
                                         int value_w,
                                         bool has_change)
{
    if (value_w <= 0) return true;

    if (!has_change) {
        return value_text.size() > static_cast<std::size_t>(value_w);
    }

    return (value_text.size() + 1 + change_text.size()) >
           static_cast<std::size_t>(value_w);
}

inline void render_metric_at(int x,
                             int logical_y,
                             int col_w,
                             int label_w,
                             const Metric& metric,
                             int body_scroll,
                             int body_top,
                             int body_height)
{
    if (x >= COLS) return;
    const int screen_y = body_top + logical_y - body_scroll;
    if (screen_y < body_top || screen_y >= body_top + body_height) return;

    std::string value_text;
    std::string change_text;
    const bool has_change =
        split_value_and_change(metric.value, &value_text, &change_text);

    const int clamped_label_w = std::max(4, label_w);
    const int raw_value_w = col_w - clamped_label_w - 2;
    const int value_w = std::max(1, raw_value_w);
    const int value_x = x + clamped_label_w + 1;

    if (metric.input_dependent) {
        const std::string_view value_for_fit =
            has_change ? std::string_view(value_text)
                       : std::string_view(metric.value);
        if (input_metric_overflows_width(value_for_fit,
                                         std::string_view(change_text),
                                         value_w,
                                         has_change)) {
            render_metric_label(screen_y, x, clamped_label_w, metric.label);
            render_metric_value(screen_y, value_x, value_w, kNaValue);
            return;
        }
    }

    if (!has_change) {
        render_metric_label(screen_y, x, clamped_label_w, metric.label);
        render_metric_value(screen_y, value_x, value_w, metric.value);
        return;
    }

    const int shown_value_w =
        std::max(1, value_w - static_cast<int>(change_text.size()) - 1);
    const int printed_value_w =
        std::min(shown_value_w, static_cast<int>(value_text.size()));
    render_metric_label(screen_y, x, clamped_label_w, metric.label);
    render_metric_value(screen_y, value_x, shown_value_w, value_text);

    const int available_change_w = std::max(0, value_w - printed_value_w - 1);
    if (available_change_w <= 0) return;
    const int printed_change_w =
        std::min(available_change_w, static_cast<int>(change_text.size()));
    const int change_x = x + clamped_label_w + 1 + printed_value_w + 1;
    if (change_x >= COLS) return;
    const short pair =
        color_pair_for_change_text(change_text, metric.invert_change_color);
    const bool use_color = has_colors() && pair > 0;
    const bool dim_zero_change = is_zero_change_text(change_text);
    if (dim_zero_change) attron(A_DIM);
    if (use_color) attron(COLOR_PAIR(pair));
    mvprintw(screen_y, change_x, "%.*s", printed_change_w, change_text.c_str());
    if (use_color) attroff(COLOR_PAIR(pair));
    if (dim_zero_change) attroff(A_DIM);
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
                 "period: %s (%d/%d)  view: %s  type: bank",
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
    const auto prev_llp_d =
        previous_row ? to_f64(previous_row->loan_loss_provisions) : std::nullopt;
    const auto prev_non_ie_d =
        previous_row ? to_f64(previous_row->non_interest_expense) : std::nullopt;
    const auto prev_rwa_d =
        previous_row ? to_f64(previous_row->risk_weighted_assets) : std::nullopt;
    const auto prev_cet1_d =
        previous_row ? to_f64(previous_row->common_equity_tier1) : std::nullopt;
    const auto prev_nco_d =
        previous_row ? to_f64(previous_row->net_charge_offs) : std::nullopt;
    const auto prev_npl_d =
        previous_row ? to_f64(previous_row->non_performing_loans) : std::nullopt;

    const auto prev_equity =
        previous_row
            ? sub_i64(previous_row->total_assets, previous_row->total_liabilities)
            : std::nullopt;
    const auto prev_equity_d = to_f64(prev_equity);
    const auto prev_tangible_equity =
        previous_row ? sub_i64(prev_equity, previous_row->goodwill) : std::nullopt;
    const auto prev_tangible_equity_d = to_f64(prev_tangible_equity);
    const auto prev_ppop =
        previous_row
            ? sub_i64(add_i64(previous_row->net_interest_income,
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
        const int all_index = find_period_index(view.all_rows, period_label(row));
        ttm_eps = ttm_sum_for_family(view.all_rows,
                                     all_index,
                                     family,
                                     ttm_window,
                                     [](const db::Database::FinanceRow& r) {
                                         return r.eps;
                                     });
        ttm_net_income_d = ttm_sum_for_family(
            view.all_rows,
            all_index,
            family,
            ttm_window,
            [](const db::Database::FinanceRow& r) { return to_f64(r.net_income); });
    }

    const bool prefer_ttm = app.settings.ttm && ttm_family_supported;
    const auto eps_for_derived =
        (prefer_ttm && is_valid_number(ttm_eps)) ? ttm_eps : eps_d_current;
    const auto net_income_for_derived = (prefer_ttm && is_valid_number(ttm_net_income_d))
                                            ? ttm_net_income_d
                                            : net_income_d;
    const auto eps_for_wished =
        (prefer_ttm && ttm_eps.has_value() && std::isfinite(*ttm_eps) &&
         *ttm_eps > 0.0)
            ? ttm_eps
            : eps_d_current;
    const auto net_income_for_wished =
        (prefer_ttm && ttm_net_income_d.has_value() &&
         std::isfinite(*ttm_net_income_d) && *ttm_net_income_d > 0.0)
            ? ttm_net_income_d
            : net_income_d;

    const auto shares_outstanding_raw =
        div_opt_nonzero(net_income_for_derived, eps_for_derived);
    const auto shares_outstanding = shares_outstanding_raw.has_value()
                                        ? std::optional<double>(std::round(
                                              *shares_outstanding_raw))
                                        : std::nullopt;
    const auto tbv_per_share =
        div_opt_nonzero(tangible_equity_d, shares_outstanding);

    const auto prev_shares_outstanding_raw =
        div_opt_nonzero(prev_net_income_d, prev_eps_d);
    const auto prev_shares_outstanding = prev_shares_outstanding_raw.has_value()
                                             ? std::optional<double>(std::round(
                                                   *prev_shares_outstanding_raw))
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

    const auto prev_roa = div_opt_nonzero(prev_net_income_d, prev_total_assets_d);
    const auto prev_rote =
        div_opt_nonzero(prev_net_income_d, prev_tangible_equity_d);
    const auto prev_ppop_to_assets = div_opt_nonzero(prev_ppop_d, prev_total_assets_d);
    const auto prev_npl_ratio = div_opt_nonzero(prev_npl_d, prev_loans_d);
    const auto prev_chargeoff_ratio = div_opt_nonzero(prev_nco_d, prev_loans_d);
    const auto prev_provision_ratio = div_opt_nonzero(prev_llp_d, prev_loans_d);
    const auto prev_provision_to_ppop = div_opt_nonzero(prev_llp_d, prev_ppop_d);
    const auto prev_cet1_ratio = div_opt_nonzero(prev_cet1_d, prev_rwa_d);
    const auto prev_leverage =
        div_opt_nonzero(prev_total_assets_d, prev_tangible_equity_d);
    const auto prev_loan_to_deposit =
        div_opt_nonzero(prev_loans_d, prev_total_deposits_d);

    const std::optional<double> typed_price = parse_decimal_input(view.inputs[0]);
    const std::optional<double> wished_per = parse_decimal_input(view.inputs[1]);
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
    const auto required_net_income_change =
        required_net_income_change_pct(required_net_income, net_income_for_wished);

    const std::vector<Metric> target_box = {
        {"P needed",
         with_change(format_compact_i64_from_f64_opt(price_needed_for_wished_per,
                                                     kNaValue),
                     price_needed_change),
         false,
         true},
        {"NI needed",
         with_change(format_compact_i64_from_f64_opt(required_net_income, kNaValue),
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
         with_change(format_i64_opt(row.total_liabilities),
                     percent_change(total_liabilities_d, prev_total_liabilities_d))},
        {"Loans",
         with_change(format_i64_opt(row.total_loans),
                     percent_change(loans_d, prev_loans_d))},
        {"Deposits",
         with_change(format_i64_opt(row.total_deposits),
                     percent_change(total_deposits_d, prev_total_deposits_d))},
        {"Goodwill",
         with_change(format_i64_opt(row.goodwill),
                     percent_change(goodwill_d, prev_goodwill_d))},
        {"Loans / Dep.",
         with_change(format_f64_opt(loan_to_deposit, true),
                     ratio_percent_change(loan_to_deposit, prev_loan_to_deposit))},
        {"E",
         with_change(format_i64_opt(equity), percent_change(equity_d, prev_equity_d))},
        {"TE",
         with_change(format_i64_opt(tangible_equity),
                     percent_change(tangible_equity_d, prev_tangible_equity_d))},
        {"Lev.",
         with_change(format_ratio_opt(leverage),
                     ratio_percent_change(leverage, prev_leverage))},
        {"TBV",
         with_change(format_ratio_opt(tbv_per_share, kNaValue),
                     ratio_percent_change(tbv_per_share, prev_tbv_per_share))},
    };

    const std::vector<Metric> earnings_box = {
        {"NII",
         with_change(format_i64_opt(row.net_interest_income),
                     percent_change(nii_d, prev_nii_d))},
        {"NonII",
         with_change(format_i64_opt(row.non_interest_income),
                     percent_change(non_ii_d, prev_non_ii_d))},
        {"NIExp",
         with_change(format_i64_opt(row.non_interest_expense),
                     percent_change(non_ie_d, prev_non_ie_d))},
        {"PPOP",
         with_change(format_i64_opt(pre_provision_profit),
                     percent_change(ppop_d, prev_ppop_d))},
        {"LLP",
         with_change(format_i64_opt(row.loan_loss_provisions),
                     percent_change(llp_d, prev_llp_d))},
        {"LLP / PPOP",
         with_change(format_f64_opt(provision_to_ppop, true),
                     ratio_percent_change(provision_to_ppop, prev_provision_to_ppop))},
        {"NI",
         with_change(format_i64_opt(row.net_income),
                     percent_change(net_income_d, prev_net_income_d))},
        {"EPS",
         with_change(format_f64_opt(row.eps), percent_change(row.eps, prev_eps_d))},
        {"ROA",
         with_change(format_f64_opt(roa, true),
                     ratio_percent_change(roa, prev_roa))},
        {"ROTE",
         with_change(format_f64_opt(rote, true),
                     ratio_percent_change(rote, prev_rote))},
        {"PPOP / A",
         with_change(format_f64_opt(ppop_to_assets, true),
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
         with_change(format_f64_opt(provision_ratio, true),
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
         with_change(format_f64_opt(chargeoff_ratio, true),
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
    constexpr int preferred_col_w = 28;
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
    const int label_w = std::clamp(col_w - 17, 6, 12);

    const std::vector<std::vector<Metric>> metric_boxes = {
        target_box,
        valuation_box,
        balance_reg_box,
        earnings_box,
        asset_quality_box,
    };

    const int first_input_y = 0;
    const int second_input_y = 1;
    const int cursor_body_y = (view.input_index == 0) ? first_input_y : second_input_y;

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
    int cursor_x = input_x + static_cast<int>(view.inputs[view.input_index].size());
    for (int i = 0; i < 2; ++i) {
        const int logical_y = first_input_y + i;
        const int screen_y = screen_y_for_body(logical_y);
        if (screen_y < body_top || screen_y >= body_top + body_height) continue;

        const bool selected = view.input_index == i;
        mvaddch(screen_y, 0, selected ? ('>' | A_BOLD) : ' ');
        mvprintw(screen_y, 2, "%-*s", input_label_w, labels[i]);
        const bool empty = view.inputs[i].empty();
        const std::string shown = (empty && !selected) ? kNaValue : view.inputs[i];
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

    if (help_lines >= 2) {
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


} // namespace views
