#pragma once
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <curses.h>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "state.hpp"

namespace views {

inline constexpr int kAddInputTab = 2;     // where labels start
inline constexpr int kAddInputCushion = 3; // spaces between label and input
inline constexpr int kAddColumnGap = 18;   // spacing between period columns
inline constexpr int kAddSecondColumnMinVisibleChars = 4;
inline constexpr int kAddSecondColumnRightPadding = 1;
inline constexpr std::size_t kAddTickerMaxLen = 12;
inline constexpr int kAddTickerType1A = 1;
inline constexpr int kAddTickerType2 = 2;
inline constexpr int kAddTickerType3 = 3;
inline constexpr int kAddTickerType1B = 4; // UI-only type 1 variant

enum class FieldKey {
    Ticker,
    Period,
    CashAndEquivalents,
    CurrentAssets,
    NonCurrentAssets,
    CurrentLiabilities,
    NonCurrentLiabilities,
    Revenue,
    NetIncome,
    Eps,
    CfoOperations,
    CfiInvesting,
    CffFinancing,
    TotalLoans,
    Goodwill,
    TotalAssets,
    TotalDeposits,
    TotalLiabilities,
    NetInterestIncome,
    NonInterestIncome,
    LoanLossProvisions,
    NonInterestExpense,
    RiskWeightedAssets,
    CommonEquityTier1,
    NetChargeOffs,
    NonPerformingLoans,
    InsuranceReserves,
    EarnedPremiums,
    ClaimsIncurred,
    InterestExpenses,
    TotalExpenses,
    UnderwritingExpenses,
    TotalDebt,
};

enum class ValueKind { Int64, Double, Text };

struct Constraint {
    ValueKind kind;
    double min;
    double max;
};

inline Constraint constraint_for_field(FieldKey key)
{
    switch (key) {
    case FieldKey::Ticker:
    case FieldKey::Period:
        return {ValueKind::Text, 0.0, 0.0};
    case FieldKey::CashAndEquivalents:
    case FieldKey::CurrentAssets:
    case FieldKey::NonCurrentAssets:
    case FieldKey::CurrentLiabilities:
    case FieldKey::NonCurrentLiabilities:
    case FieldKey::Revenue:
    case FieldKey::TotalLoans:
    case FieldKey::Goodwill:
    case FieldKey::TotalAssets:
    case FieldKey::TotalDeposits:
    case FieldKey::TotalLiabilities:
    case FieldKey::RiskWeightedAssets:
    case FieldKey::CommonEquityTier1:
    case FieldKey::NonPerformingLoans:
    case FieldKey::InsuranceReserves:
    case FieldKey::EarnedPremiums:
    case FieldKey::ClaimsIncurred:
    case FieldKey::InterestExpenses:
    case FieldKey::TotalExpenses:
    case FieldKey::UnderwritingExpenses:
    case FieldKey::TotalDebt:
        return {ValueKind::Int64, 0.0, 1e14};
    case FieldKey::NetIncome:
    case FieldKey::CfoOperations:
    case FieldKey::CfiInvesting:
    case FieldKey::CffFinancing:
    case FieldKey::NetInterestIncome:
    case FieldKey::NonInterestIncome:
    case FieldKey::LoanLossProvisions:
    case FieldKey::NonInterestExpense:
    case FieldKey::NetChargeOffs:
        return {ValueKind::Int64, -1e14, 1e14};
    case FieldKey::Eps:
        return {ValueKind::Double, -1e5, 1e5};
    }
    return {ValueKind::Text, 0.0, 0.0};
}

inline std::string sanitize_ticker(std::string_view s)
{
    std::string out;
    out.reserve(kAddTickerMaxLen);

    char prev = '\0';
    for (char ch : s) {
        if ((ch >= 'a' && ch <= 'z')) ch = char(ch - 'a' + 'A');

        const bool is_alnum =
            (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        const bool is_dot = (ch == '.');

        if (!is_alnum && !is_dot) continue;
        if (is_dot && prev == '.') continue;

        out.push_back(ch);
        prev = ch;
        if (out.size() >= kAddTickerMaxLen) break;
    }

    return out;
}

inline bool period_ok(std::string_view s)
{
    static const std::regex re(R"(^\d{4}-(?:Y|Q[1-4]|S[1-2])$)",
                               std::regex::ECMAScript | std::regex::icase);
    return std::regex_match(s.begin(), s.end(), re);
}

inline std::string trim_copy(std::string_view s)
{
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
        ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    return std::string(s.substr(b, e - b));
}

inline bool parse_int64(std::string_view s, std::int64_t* out)
{
    std::string t = trim_copy(s);
    if (t.empty()) return false;

    size_t i = 0;
    if (t[0] == '+' || t[0] == '-') i = 1;
    if (i == t.size()) return false;

    for (; i < t.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(t[i]))) return false;
    }

    try {
        const long long v = std::stoll(t);
        *out = static_cast<std::int64_t>(v);
        return true;
    }
    catch (...) {
        return false;
    }
}

inline bool parse_double(std::string_view s, double* out)
{
    std::string t = trim_copy(s);
    if (t.empty()) return false;

    try {
        size_t pos = 0;
        const double v = std::stod(t, &pos);
        if (pos != t.size()) return false;
        *out = v;
        return true;
    }
    catch (...) {
        return false;
    }
}

inline bool
validate_and_parse(std::string_view raw, FieldKey key, AddState::OptValue* out)
{
    std::string t = trim_copy(raw);
    const Constraint cons = constraint_for_field(key);

    if (cons.kind == ValueKind::Text) {
        if (t.empty()) return false;

        if (key == FieldKey::Ticker) {
            std::string s = sanitize_ticker(t);
            if (s.empty()) return false;
            *out = AddState::Value{std::move(s)};
            return true;
        }

        if (key == FieldKey::Period) {
            std::transform(t.begin(), t.end(), t.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            if (!period_ok(t)) return false;
            *out = AddState::Value{t};
            return true;
        }

        return false;
    }

    if (t.empty()) {
        *out = std::nullopt;
        return true;
    }

    if (cons.kind == ValueKind::Int64) {
        std::int64_t v = 0;
        if (!parse_int64(t, &v)) return false;
        const double dv = static_cast<double>(v);
        if (dv < cons.min || dv > cons.max) return false;
        *out = AddState::Value{v};
        return true;
    }

    double v = 0.0;
    if (!parse_double(t, &v)) return false;
    if (v < cons.min || v > cons.max) return false;
    *out = AddState::Value{v};
    return true;
}

struct AddField {
    FieldKey key;
    const char* label;
};

struct AddSection {
    int field_index;
    const char* title;
};

inline bool is_supported_add_ticker_type(int ticker_type)
{
    return ticker_type == kAddTickerType1A || ticker_type == kAddTickerType2 ||
           ticker_type == kAddTickerType3 || ticker_type == kAddTickerType1B;
}

inline int normalize_add_ticker_type(int ticker_type)
{
    return is_supported_add_ticker_type(ticker_type) ? ticker_type
                                                     : kAddTickerType1A;
}

inline bool is_type1_add_variant(int ticker_type)
{
    ticker_type = normalize_add_ticker_type(ticker_type);
    return ticker_type == kAddTickerType1A || ticker_type == kAddTickerType1B;
}

inline int add_ticker_type_to_db_type(int ticker_type)
{
    ticker_type = normalize_add_ticker_type(ticker_type);
    if (ticker_type == kAddTickerType1B) return kAddTickerType1A;
    return ticker_type;
}

inline int next_add_ticker_type(int ticker_type)
{
    ticker_type = normalize_add_ticker_type(ticker_type);
    if (ticker_type == kAddTickerType1A) return kAddTickerType1B;
    if (ticker_type == kAddTickerType1B) return kAddTickerType2;
    if (ticker_type == kAddTickerType2) return kAddTickerType3;
    return kAddTickerType1A;
}

inline const std::vector<AddField>& add_fields_for_type(int ticker_type)
{
    static const std::vector<AddField> f_type1a = {
        {FieldKey::Ticker, "ticker"},
        {FieldKey::Period, "period"},

        {FieldKey::CashAndEquivalents, "cash"},
        {FieldKey::CurrentAssets, "current assets"},
        {FieldKey::NonCurrentAssets, "non-current assets"},
        {FieldKey::CurrentLiabilities, "current liab."},
        {FieldKey::NonCurrentLiabilities, "non-current liab."},

        {FieldKey::Revenue, "revenue"},
        {FieldKey::NetIncome, "net inc."},
        {FieldKey::Eps, "eps"},

        {FieldKey::CfoOperations, "operations"},
        {FieldKey::CfiInvesting, "investing"},
        {FieldKey::CffFinancing, "financing"},
    };

    static const std::vector<AddField> f_type1b = {
        {FieldKey::Ticker, "ticker"},
        {FieldKey::Period, "period"},

        {FieldKey::CashAndEquivalents, "cash"},
        {FieldKey::CurrentAssets, "current assets"},
        {FieldKey::TotalAssets, "total assets"},
        {FieldKey::CurrentLiabilities, "current liab."},
        {FieldKey::TotalLiabilities, "total liab."},

        {FieldKey::Revenue, "revenue"},
        {FieldKey::NetIncome, "net inc."},
        {FieldKey::Eps, "eps"},

        {FieldKey::CfoOperations, "operations"},
        {FieldKey::CfiInvesting, "investing"},
        {FieldKey::CffFinancing, "financing"},
    };

    static const std::vector<AddField> f_type2 = {
        {FieldKey::Ticker, "ticker"},
        {FieldKey::Period, "period"},

        {FieldKey::TotalLoans, "loans"},
        {FieldKey::Goodwill, "goodwill"},
        {FieldKey::TotalAssets, "assets"},
        {FieldKey::TotalDeposits, "deposits"},
        {FieldKey::TotalLiabilities, "liab."},

        {FieldKey::NetInterestIncome, "nii"},
        {FieldKey::NonInterestIncome, "non-int inc."},
        {FieldKey::LoanLossProvisions, "llp"},
        {FieldKey::NonInterestExpense, "non-int exp."},
        {FieldKey::NetIncome, "net inc."},
        {FieldKey::Eps, "eps"},

        {FieldKey::RiskWeightedAssets, "rwa"},
        {FieldKey::CommonEquityTier1, "cet1"},

        {FieldKey::NetChargeOffs, "nco"},
        {FieldKey::NonPerformingLoans, "npl"},
    };

    static const std::vector<AddField> f_type3 = {
        {FieldKey::Ticker, "ticker"},
        {FieldKey::Period, "period"},

        {FieldKey::TotalAssets, "assets"},
        {FieldKey::InsuranceReserves, "reserves"},
        {FieldKey::TotalDebt, "debt"},
        {FieldKey::TotalLiabilities, "liab."},

        {FieldKey::EarnedPremiums, "premiums"},
        {FieldKey::ClaimsIncurred, "claims"},
        {FieldKey::InterestExpenses, "interests"},
        {FieldKey::TotalExpenses, "expenses"},
        {FieldKey::NetIncome, "net inc."},
        {FieldKey::Eps, "eps"},
    };

    ticker_type = normalize_add_ticker_type(ticker_type);
    if (ticker_type == kAddTickerType2) return f_type2;
    if (ticker_type == kAddTickerType3) return f_type3;
    if (ticker_type == kAddTickerType1B) return f_type1b;
    return f_type1a;
}

inline const std::vector<AddSection>& add_sections_for_type(int ticker_type)
{
    static const std::vector<AddSection> s_type1 = {
        {2, "BALANCE"},
        {7, "INCOME"},
        {10, "CASH FLOW"},
    };

    static const std::vector<AddSection> s_type2 = {
        {2, "BALANCE"},
        {7, "INCOME"},
        {13, "REGULATORY"},
        {15, "OTHERS"},
    };

    static const std::vector<AddSection> s_type3 = {
        {2, "BALANCE"},
        {6, "INCOME"},
    };

    ticker_type = normalize_add_ticker_type(ticker_type);
    if (ticker_type == kAddTickerType2) return s_type2;
    if (ticker_type == kAddTickerType3) return s_type3;
    return s_type1;
}

inline int add_input_x_for_type(int ticker_type)
{
    auto compute = [](const auto& fields) {
        std::size_t max_len = 0;
        for (const auto& f : fields) {
            if (f.label) max_len = std::max(max_len, std::strlen(f.label));
        }
        return kAddInputTab + static_cast<int>(max_len) + kAddInputCushion;
    };

    static const int x1a = compute(add_fields_for_type(kAddTickerType1A));
    static const int x1b = compute(add_fields_for_type(kAddTickerType1B));
    static const int x2 = compute(add_fields_for_type(kAddTickerType2));
    static const int x3 = compute(add_fields_for_type(kAddTickerType3));
    ticker_type = normalize_add_ticker_type(ticker_type);
    if (ticker_type == kAddTickerType2) return x2;
    if (ticker_type == kAddTickerType3) return x3;
    if (ticker_type == kAddTickerType1B) return x1b;
    return x1a;
}

inline int add_input_x(const AppState& app)
{
    return add_input_x_for_type(app.add.ticker_type);
}

inline int add_input_x_for_column(int base_input_x, int column)
{
    if (column <= 0) return base_input_x;
    return base_input_x + (kAddColumnGap * column);
}

inline int add_min_cols_for_two_column_mode(int base_input_x)
{
    const int second_input_x = add_input_x_for_column(base_input_x, 1);
    return second_input_x + kAddSecondColumnMinVisibleChars +
           kAddSecondColumnRightPadding;
}

inline bool add_terminal_too_narrow_for_two_column_mode(int term_cols,
                                                        int base_input_x)
{
    if (term_cols <= 0) return false;
    return term_cols < add_min_cols_for_two_column_mode(base_input_x);
}

inline int add_field_index(int ticker_type, FieldKey key)
{
    const auto& fields = add_fields_for_type(ticker_type);
    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        if (fields[static_cast<std::size_t>(i)].key == key) return i;
    }
    return -1;
}

inline void sync_add_secondary_ticker(AppState& app);

inline std::string format_double_3(double x)
{
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(3) << x;
    std::string s = oss.str();

    while (!s.empty() && s.back() == '0')
        s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

inline std::string opt_i64_to_input(std::optional<std::int64_t> v)
{
    if (!v) return {};
    return std::to_string(*v);
}

inline std::string opt_f64_to_input(std::optional<double> v)
{
    if (!v) return {};
    return format_double_3(*v);
}

inline std::string add_period_label(const db::Database::FinanceRow& row)
{
    return std::to_string(row.year) + "-" + row.period_type;
}

inline int
add_find_period_index(const std::vector<db::Database::FinanceRow>& rows,
                      const std::string& period)
{
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (add_period_label(rows[static_cast<std::size_t>(i)]) == period) {
            return i;
        }
    }
    return -1;
}

inline void clamp_add_index(AppState& app, int field_count)
{
    if (field_count <= 0) {
        app.add.index = 0;
        return;
    }
    if (app.add.index < 0) app.add.index = 0;
    if (app.add.index >= field_count) app.add.index = field_count - 1;
}

inline void reset_add_form_for_type(AppState& app,
                                    int ticker_type,
                                    bool ticker_type_locked,
                                    AddMode mode)
{
    ticker_type = normalize_add_ticker_type(ticker_type);
    app.add.reset(static_cast<int>(add_fields_for_type(ticker_type).size()));
    app.add.mode = mode;
    app.add.ticker_type = ticker_type;
    app.add.ticker_type_locked = ticker_type_locked;
}

inline void open_add_create(AppState& app)
{
    reset_add_form_for_type(app, kAddTickerType1A, false, AddMode::Create);
    sync_add_secondary_ticker(app);
    app.current = views::ViewId::Add;
}

inline void open_add_prefilled_from_ticker(AppState& app,
                                           const db::Database::FinanceRow& row)
{
    const int ticker_type =
        normalize_add_ticker_type(app.ticker_view.ticker_type);
    reset_add_form_for_type(app, ticker_type, true, AddMode::EditFromTicker);
    // Edit mode reuses the add form with fields prefilled from the selected
    // period.
    auto set_buffer = [&](FieldKey key, const std::string& value) {
        const int idx = add_field_index(app.add.ticker_type, key);
        if (idx >= 0) app.add.buffers[static_cast<std::size_t>(idx)] = value;
    };

    set_buffer(FieldKey::Ticker, row.ticker);
    set_buffer(FieldKey::Period, add_period_label(row));

    if (app.add.ticker_type == 2) {
        set_buffer(FieldKey::TotalLoans, opt_i64_to_input(row.total_loans));
        set_buffer(FieldKey::Goodwill, opt_i64_to_input(row.goodwill));
        set_buffer(FieldKey::TotalAssets, opt_i64_to_input(row.total_assets));
        set_buffer(FieldKey::TotalDeposits,
                   opt_i64_to_input(row.total_deposits));
        set_buffer(FieldKey::TotalLiabilities,
                   opt_i64_to_input(row.total_liabilities));
        set_buffer(FieldKey::NetInterestIncome,
                   opt_i64_to_input(row.net_interest_income));
        set_buffer(FieldKey::NonInterestIncome,
                   opt_i64_to_input(row.non_interest_income));
        set_buffer(FieldKey::LoanLossProvisions,
                   opt_i64_to_input(row.loan_loss_provisions));
        set_buffer(FieldKey::NonInterestExpense,
                   opt_i64_to_input(row.non_interest_expense));
        set_buffer(FieldKey::NetIncome, opt_i64_to_input(row.net_income));
        set_buffer(FieldKey::Eps, opt_f64_to_input(row.eps));
        set_buffer(FieldKey::RiskWeightedAssets,
                   opt_i64_to_input(row.risk_weighted_assets));
        set_buffer(FieldKey::CommonEquityTier1,
                   opt_i64_to_input(row.common_equity_tier1));
        set_buffer(FieldKey::NetChargeOffs,
                   opt_i64_to_input(row.net_charge_offs));
        set_buffer(FieldKey::NonPerformingLoans,
                   opt_i64_to_input(row.non_performing_loans));
    }
    else if (app.add.ticker_type == 3) {
        set_buffer(FieldKey::TotalAssets, opt_i64_to_input(row.total_assets));
        set_buffer(FieldKey::InsuranceReserves,
                   opt_i64_to_input(row.insurance_reserves));
        set_buffer(FieldKey::TotalDebt, opt_i64_to_input(row.total_debt));
        set_buffer(FieldKey::TotalLiabilities,
                   opt_i64_to_input(row.total_liabilities));
        set_buffer(FieldKey::EarnedPremiums,
                   opt_i64_to_input(row.earned_premiums));
        set_buffer(FieldKey::ClaimsIncurred,
                   opt_i64_to_input(row.claims_incurred));
        set_buffer(FieldKey::InterestExpenses,
                   opt_i64_to_input(row.interest_expenses));
        std::optional<std::int64_t> prefill_total_expenses = row.total_expenses;
        if (!prefill_total_expenses.has_value() &&
            row.claims_incurred.has_value() &&
            row.underwriting_expenses.has_value()) {
            prefill_total_expenses = *row.claims_incurred +
                                     *row.underwriting_expenses +
                                     row.interest_expenses.value_or(0);
        }
        set_buffer(FieldKey::TotalExpenses,
                   opt_i64_to_input(prefill_total_expenses));
        set_buffer(FieldKey::NetIncome, opt_i64_to_input(row.net_income));
        set_buffer(FieldKey::Eps, opt_f64_to_input(row.eps));
    }
    else {
        set_buffer(FieldKey::CashAndEquivalents,
                   opt_i64_to_input(row.cash_and_equivalents));
        set_buffer(FieldKey::CurrentAssets,
                   opt_i64_to_input(row.current_assets));
        set_buffer(FieldKey::NonCurrentAssets,
                   opt_i64_to_input(row.non_current_assets));
        set_buffer(FieldKey::CurrentLiabilities,
                   opt_i64_to_input(row.current_liabilities));
        set_buffer(FieldKey::NonCurrentLiabilities,
                   opt_i64_to_input(row.non_current_liabilities));
        set_buffer(FieldKey::Revenue, opt_i64_to_input(row.revenue));
        set_buffer(FieldKey::NetIncome, opt_i64_to_input(row.net_income));
        set_buffer(FieldKey::Eps, opt_f64_to_input(row.eps));
        set_buffer(FieldKey::CfoOperations,
                   opt_i64_to_input(row.cash_flow_from_operations));
        set_buffer(FieldKey::CfiInvesting,
                   opt_i64_to_input(row.cash_flow_from_investing));
        set_buffer(FieldKey::CffFinancing,
                   opt_i64_to_input(row.cash_flow_from_financing));
    }

    sync_add_secondary_ticker(app);
    app.current = views::ViewId::Add;
}

inline void ensure_add_initialized(AppState& app)
{
    app.add.ticker_type = normalize_add_ticker_type(app.add.ticker_type);
    const int field_count =
        static_cast<int>(add_fields_for_type(app.add.ticker_type).size());
    if (!app.add.active) {
        reset_add_form_for_type(
            app, app.add.ticker_type, app.add.ticker_type_locked, app.add.mode);
        return;
    }

    if (static_cast<int>(app.add.buffers.size()) != field_count ||
        static_cast<int>(app.add.buffers_extra.size()) != field_count ||
        static_cast<int>(app.add.values.size()) != field_count ||
        static_cast<int>(app.add.values_extra.size()) != field_count ||
        static_cast<int>(app.add.layout_y.size()) != field_count) {
        reset_add_form_for_type(
            app, app.add.ticker_type, app.add.ticker_type_locked, app.add.mode);
        app.add.active = true;
        return;
    }

    if (app.add.mode == AddMode::EditFromTicker) {
        app.add.value_columns = 1;
        app.add.column = 0;
    }
    else {
        app.add.value_columns = std::clamp(app.add.value_columns, 1, 2);
        app.add.column =
            std::clamp(app.add.column, 0, app.add.value_columns - 1);
    }
    sync_add_secondary_ticker(app);
}

inline void clamp_add_cursor(AppState& app)
{
    const auto& buffers =
        (app.add.column <= 0) ? app.add.buffers : app.add.buffers_extra;
    if (buffers.empty()) {
        app.add.cursor = 0;
        return;
    }

    clamp_add_index(app, static_cast<int>(buffers.size()));
    const auto& buf = buffers[app.add.index];
    if (app.add.cursor < 0) app.add.cursor = 0;
    if (app.add.cursor > static_cast<int>(buf.size())) {
        app.add.cursor = static_cast<int>(buf.size());
    }
}

inline void normalize_field_buffer(FieldKey key, std::string& buf, int* cursor)
{
    int next_cursor = cursor ? *cursor : static_cast<int>(buf.size());
    if (next_cursor < 0) next_cursor = 0;
    if (next_cursor > static_cast<int>(buf.size())) {
        next_cursor = static_cast<int>(buf.size());
    }

    if (key == FieldKey::Ticker) {
        const std::string before =
            buf.substr(0, static_cast<std::size_t>(next_cursor));
        const std::string sanitized_before = sanitize_ticker(before);
        buf = sanitize_ticker(buf);
        next_cursor = static_cast<int>(sanitized_before.size());
    }
    else if (key == FieldKey::Period) {
        std::transform(
            buf.begin(), buf.end(), buf.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
        if (buf.size() > 8) buf.resize(8);
    }

    if (next_cursor > static_cast<int>(buf.size())) {
        next_cursor = static_cast<int>(buf.size());
    }
    if (cursor) *cursor = next_cursor;
}

inline bool is_allowed_char_for_current_field(int ch,
                                              FieldKey key,
                                              const std::string& buf,
                                              int cursor)
{
    if (ch < 0 || ch > 255) return false;
    const unsigned char c = static_cast<unsigned char>(ch);

    const Constraint cons = constraint_for_field(key);

    if (cons.kind == ValueKind::Text) {
        if (key == FieldKey::Ticker) {
            return std::isalnum(c) || c == '.';
        }
        if (key == FieldKey::Period) {
            if (std::isdigit(c) || c == '-') return true;
            return c == 'Y' || c == 'y' || c == 'Q' || c == 'q' || c == 'S' ||
                   c == 's';
        }
        return false;
    }

    const bool allow_negative = cons.min < 0.0;

    if (std::isdigit(c)) {
        if (cons.kind == ValueKind::Int64) {
            int digit_count =
                std::count_if(buf.begin(), buf.end(), [](unsigned char c2) {
                    return std::isdigit(c2);
                });
            if (digit_count >= 15) return false;
        }
        return true;
    }

    if (cons.kind == ValueKind::Double) {
        if (c == '.' && buf.find('.') == std::string::npos) return true;
    }

    if (allow_negative) {
        if (c == '-' && cursor == 0 && buf.find('-') == std::string::npos)
            return true;
    }

    return false;
}

inline bool validate_add_form(const std::vector<AddField>& fields,
                              const std::vector<std::string>& buffers,
                              std::vector<AddState::OptValue>* out_values,
                              int* invalid_index)
{
    std::vector<AddState::OptValue> parsed(fields.size());

    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        if (!validate_and_parse(buffers[static_cast<std::size_t>(i)],
                                fields[static_cast<std::size_t>(i)].key,
                                &parsed[static_cast<std::size_t>(i)])) {
            if (invalid_index) *invalid_index = i;
            return false;
        }
    }

    if (out_values) *out_values = std::move(parsed);
    if (invalid_index) *invalid_index = -1;
    return true;
}

inline std::optional<std::string> add_ticker_buffer(const AppState& app)
{
    const int idx = add_field_index(app.add.ticker_type, FieldKey::Ticker);
    if (idx < 0 || idx >= static_cast<int>(app.add.buffers.size()))
        return std::nullopt;
    return app.add.buffers[static_cast<std::size_t>(idx)];
}

inline void sync_add_secondary_ticker(AppState& app)
{
    const int ticker_idx =
        add_field_index(app.add.ticker_type, FieldKey::Ticker);
    if (ticker_idx < 0 ||
        ticker_idx >= static_cast<int>(app.add.buffers.size()) ||
        ticker_idx >= static_cast<int>(app.add.buffers_extra.size())) {
        return;
    }
    app.add.buffers_extra[static_cast<std::size_t>(ticker_idx)] =
        app.add.buffers[static_cast<std::size_t>(ticker_idx)];
}

inline bool sync_add_type_lock_from_ticker(AppState& app)
{
    if (app.add.mode != AddMode::Create) return true;

    const auto ticker_buf = add_ticker_buffer(app);
    if (!ticker_buf.has_value() || ticker_buf->empty()) {
        app.add.ticker_type_locked = false;
        return true;
    }

    std::string err;
    const auto db_type = app.db->get_ticker_type(*ticker_buf, &err);
    if (!err.empty()) {
        route_error(app, err);
        return true;
    }

    if (!db_type.has_value()) {
        app.add.ticker_type_locked = false;
        return true;
    }

    const int locked_type = normalize_add_ticker_type(*db_type);
    if (add_ticker_type_to_db_type(app.add.ticker_type) == locked_type) {
        app.add.ticker_type_locked = true;
        return true;
    }

    const std::string keep_ticker = *ticker_buf;
    reset_add_form_for_type(app, locked_type, true, AddMode::Create);
    const int ticker_idx =
        add_field_index(app.add.ticker_type, FieldKey::Ticker);
    if (ticker_idx >= 0) {
        app.add.buffers[static_cast<std::size_t>(ticker_idx)] = keep_ticker;
        app.add.index = ticker_idx;
        app.add.cursor = static_cast<int>(keep_ticker.size());
    }
    sync_add_secondary_ticker(app);
    return true;
}

inline void cycle_add_type_and_clear(AppState& app)
{
    if (app.add.mode != AddMode::Create) return;

    if (app.add.ticker_type_locked) {
        if (add_ticker_type_to_db_type(app.add.ticker_type) !=
            kAddTickerType1A) {
            return;
        }

        const int next_type = (app.add.ticker_type == kAddTickerType1A)
                                  ? kAddTickerType1B
                                  : kAddTickerType1A;
        const std::string keep_ticker = add_ticker_buffer(app).value_or("");
        reset_add_form_for_type(app, next_type, true, AddMode::Create);
        const int ticker_idx =
            add_field_index(app.add.ticker_type, FieldKey::Ticker);
        if (ticker_idx >= 0) {
            app.add.buffers[static_cast<std::size_t>(ticker_idx)] = keep_ticker;
            app.add.index = ticker_idx;
            app.add.cursor = static_cast<int>(keep_ticker.size());
        }
        sync_add_secondary_ticker(app);
        return;
    }

    const int next_type = next_add_ticker_type(app.add.ticker_type);
    reset_add_form_for_type(app, next_type, false, AddMode::Create);
    sync_add_secondary_ticker(app);
}

} // namespace views


