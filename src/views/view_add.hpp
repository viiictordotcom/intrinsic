#pragma once
#include <algorithm>
#include <cctype>
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
inline constexpr std::size_t kAddTickerMaxLen = 12;

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
    return ticker_type == 1 || ticker_type == 2 || ticker_type == 3;
}

inline int normalize_add_ticker_type(int ticker_type)
{
    return is_supported_add_ticker_type(ticker_type) ? ticker_type : 1;
}

inline int next_add_ticker_type(int ticker_type)
{
    ticker_type = normalize_add_ticker_type(ticker_type);
    if (ticker_type == 1) return 2;
    if (ticker_type == 2) return 3;
    return 1;
}

inline const std::vector<AddField>& add_fields_for_type(int ticker_type)
{
    static const std::vector<AddField> f_type1 = {
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
    if (ticker_type == 2) return f_type2;
    if (ticker_type == 3) return f_type3;
    return f_type1;
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
    if (ticker_type == 2) return s_type2;
    if (ticker_type == 3) return s_type3;
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

    static const int x1 = compute(add_fields_for_type(1));
    static const int x2 = compute(add_fields_for_type(2));
    static const int x3 = compute(add_fields_for_type(3));
    ticker_type = normalize_add_ticker_type(ticker_type);
    if (ticker_type == 2) return x2;
    if (ticker_type == 3) return x3;
    return x1;
}

inline int add_input_x(const AppState& app)
{
    return add_input_x_for_type(app.add.ticker_type);
}

inline int add_field_index(int ticker_type, FieldKey key)
{
    const auto& fields = add_fields_for_type(ticker_type);
    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        if (fields[static_cast<std::size_t>(i)].key == key) return i;
    }
    return -1;
}

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
    reset_add_form_for_type(app, 1, false, AddMode::Create);
    app.current = views::ViewId::Add;
}

inline void open_add_prefilled_from_ticker(AppState& app,
                                           const db::Database::FinanceRow& row)
{
    const int ticker_type = normalize_add_ticker_type(app.ticker_view.ticker_type);
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
        set_buffer(FieldKey::TotalDeposits, opt_i64_to_input(row.total_deposits));
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
        set_buffer(FieldKey::NetChargeOffs, opt_i64_to_input(row.net_charge_offs));
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
        if (!prefill_total_expenses.has_value() && row.claims_incurred.has_value() &&
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
        set_buffer(FieldKey::CurrentAssets, opt_i64_to_input(row.current_assets));
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
        static_cast<int>(app.add.values.size()) != field_count ||
        static_cast<int>(app.add.layout_y.size()) != field_count) {
        reset_add_form_for_type(
            app, app.add.ticker_type, app.add.ticker_type_locked, app.add.mode);
        app.add.active = true;
    }
}

inline void clamp_add_cursor(AppState& app)
{
    if (app.add.buffers.empty()) {
        app.add.cursor = 0;
        return;
    }

    clamp_add_index(app, static_cast<int>(app.add.buffers.size()));
    const auto& buf = app.add.buffers[app.add.index];
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
    if (locked_type == app.add.ticker_type) {
        app.add.ticker_type_locked = true;
        return true;
    }

    const std::string keep_ticker = *ticker_buf;
    reset_add_form_for_type(app, locked_type, true, AddMode::Create);
    const int ticker_idx = add_field_index(app.add.ticker_type, FieldKey::Ticker);
    if (ticker_idx >= 0) {
        app.add.buffers[static_cast<std::size_t>(ticker_idx)] = keep_ticker;
        app.add.index = ticker_idx;
        app.add.cursor = static_cast<int>(keep_ticker.size());
    }
    return true;
}

inline void cycle_add_type_and_clear(AppState& app)
{
    if (app.add.mode != AddMode::Create || app.add.ticker_type_locked) return;
    const int next_type = next_add_ticker_type(app.add.ticker_type);
    reset_add_form_for_type(app, next_type, false, AddMode::Create);
}

inline void render_add(AppState& app);

inline void flash_add_invalid_marker(AppState& app, int input_x)
{
    render_add(app);
    const int y = app.add.layout_y[app.add.index];
    if (y >= 0 && y < LINES) {
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

    const int input_x = add_input_x(app);
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
        suffix += " t" + std::to_string(app.add.ticker_type);
        if (app.add.ticker_type == 1) {
            suffix += " (default)";
        }
        else if (app.add.ticker_type == 2) {
            suffix += " (bank)";
        }
        else if (app.add.ticker_type == 3) {
            suffix += " (insurer)";
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
        const int cursor_x = std::min(input_x + app.add.cursor, std::max(0, COLS - 1));
        if (line_y >= 0 && line_y < viewport) {
            desired_cursor_y = line_y;
            desired_cursor_x = cursor_x;
        }
        curs_set(1);
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

    if (LINES > 0 && total_lines < LINES) {
        const int hint_y =
            (bottom_status_y == LINES - 1) ? (LINES - 2) : (LINES - 1);
        if (hint_y >= 0) {
            std::string hint = "enter: confirm   esc: cancel";
            if (app.add.mode == AddMode::Create) {
                if (app.add.ticker_type_locked) {
                    hint += "   space: locked by ticker";
                }
                else {
                    hint += "   space: switch type";
                }
            }
            else {
                hint += "   space: type locked";
            }
            attron(A_DIM);
            mvprintw(hint_y, 0, "%.*s", std::max(0, COLS - 1), hint.c_str());
            attroff(A_DIM);
        }
    }

    // Move the cursor after drawing hints/status so it stays on the active input.
    if (!app.add.confirming && desired_cursor_y >= 0 && desired_cursor_y < viewport) {
        move(desired_cursor_y, desired_cursor_x);
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

inline const AddState::OptValue* add_value_for_key(const AppState& app, FieldKey key)
{
    const int idx = add_field_index(app.add.ticker_type, key);
    if (idx < 0 || idx >= static_cast<int>(app.add.values.size())) return nullptr;
    return &app.add.values[static_cast<std::size_t>(idx)];
}

inline bool handle_key_add(AppState& app, int ch)
{
    ensure_add_initialized(app);

    const int input_x = add_input_x(app);
    const auto& fields = add_fields_for_type(app.add.ticker_type);
    clamp_add_index(app, static_cast<int>(fields.size()));
    clamp_add_cursor(app);

    if (ch == 27 /*ESC*/) {
        app.add.active = false;
        app.current = (app.add.mode == AddMode::EditFromTicker)
                          ? views::ViewId::Ticker
                          : views::ViewId::Home;
        return true;
    }

    if (app.add.confirming) {
        if (ch == 'y' || ch == 'Y') {
            const AddState::OptValue* ticker_v =
                add_value_for_key(app, FieldKey::Ticker);
            const AddState::OptValue* period_v =
                add_value_for_key(app, FieldKey::Period);
            auto ticker_opt = ticker_v ? as_str_opt(*ticker_v) : std::nullopt;
            auto period_opt = period_v ? as_str_opt(*period_v) : std::nullopt;
            if (!ticker_opt || !period_opt) {
                route_error(app, "ticker/period missing");
                return true;
            }

            db::Database::FinancePayload payload{};
            auto i64_for = [&](FieldKey key) -> std::optional<std::int64_t> {
                const AddState::OptValue* v = add_value_for_key(app, key);
                return v ? as_i64_opt(*v) : std::nullopt;
            };
            auto f64_for = [&](FieldKey key) -> std::optional<double> {
                const AddState::OptValue* v = add_value_for_key(app, key);
                return v ? as_f64_opt(*v) : std::nullopt;
            };

            payload.net_income = i64_for(FieldKey::NetIncome);
            payload.eps = f64_for(FieldKey::Eps);

            if (app.add.ticker_type == 2) {
                payload.total_loans = i64_for(FieldKey::TotalLoans);
                payload.goodwill = i64_for(FieldKey::Goodwill);
                payload.total_assets = i64_for(FieldKey::TotalAssets);
                payload.total_deposits = i64_for(FieldKey::TotalDeposits);
                payload.total_liabilities = i64_for(FieldKey::TotalLiabilities);
                payload.net_interest_income =
                    i64_for(FieldKey::NetInterestIncome);
                payload.non_interest_income =
                    i64_for(FieldKey::NonInterestIncome);
                payload.loan_loss_provisions =
                    i64_for(FieldKey::LoanLossProvisions);
                payload.non_interest_expense =
                    i64_for(FieldKey::NonInterestExpense);
                payload.risk_weighted_assets =
                    i64_for(FieldKey::RiskWeightedAssets);
                payload.common_equity_tier1 =
                    i64_for(FieldKey::CommonEquityTier1);
                payload.net_charge_offs = i64_for(FieldKey::NetChargeOffs);
                payload.non_performing_loans =
                    i64_for(FieldKey::NonPerformingLoans);
            }
            else if (app.add.ticker_type == 3) {
                payload.total_assets = i64_for(FieldKey::TotalAssets);
                payload.insurance_reserves =
                    i64_for(FieldKey::InsuranceReserves);
                payload.total_debt = i64_for(FieldKey::TotalDebt);
                payload.total_liabilities = i64_for(FieldKey::TotalLiabilities);
                payload.earned_premiums = i64_for(FieldKey::EarnedPremiums);
                payload.claims_incurred = i64_for(FieldKey::ClaimsIncurred);
                payload.interest_expenses =
                    i64_for(FieldKey::InterestExpenses);
                payload.total_expenses = i64_for(FieldKey::TotalExpenses);
                if (payload.total_expenses.has_value() &&
                    payload.claims_incurred.has_value()) {
                    payload.underwriting_expenses =
                        *payload.total_expenses - *payload.claims_incurred -
                        payload.interest_expenses.value_or(0);
                }
            }
            else {
                payload.cash_and_equivalents =
                    i64_for(FieldKey::CashAndEquivalents);
                payload.current_assets = i64_for(FieldKey::CurrentAssets);
                payload.non_current_assets =
                    i64_for(FieldKey::NonCurrentAssets);
                payload.current_liabilities =
                    i64_for(FieldKey::CurrentLiabilities);
                payload.non_current_liabilities =
                    i64_for(FieldKey::NonCurrentLiabilities);
                payload.revenue = i64_for(FieldKey::Revenue);
                payload.cash_flow_from_operations =
                    i64_for(FieldKey::CfoOperations);
                payload.cash_flow_from_investing =
                    i64_for(FieldKey::CfiInvesting);
                payload.cash_flow_from_financing =
                    i64_for(FieldKey::CffFinancing);
            }

            std::string err;
            if (!app.db->add_finances(
                    *ticker_opt, *period_opt, payload, &err, app.add.ticker_type)) {
                route_error(app, err);
                return true;
            }

            app.tickers.invalidate_prefetch(); // refresh home ordering

            if (app.add.mode == AddMode::EditFromTicker) {
                auto refreshed = app.db->get_finances(*ticker_opt, &err);
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
                    *ticker_opt, std::move(refreshed), app.add.ticker_type);
                const int idx =
                    add_find_period_index(app.ticker_view.rows, *period_opt);
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

    if (ch == '\t'
#ifdef KEY_TAB
        || ch == KEY_TAB
#endif
    ) {
        if (app.add.index + 1 < static_cast<int>(fields.size()))
            app.add.index += 1;
        clamp_add_cursor(app);
        return true;
    }

    if (ch == KEY_UP) {
        if (app.add.index > 0) app.add.index -= 1;
        clamp_add_cursor(app);
        return true;
    }

    if (ch == KEY_DOWN) {
        if (app.add.index + 1 < static_cast<int>(fields.size()))
            app.add.index += 1;
        clamp_add_cursor(app);
        return true;
    }

    if (ch == KEY_LEFT) {
        if (app.add.cursor > 0) app.add.cursor -= 1;
        return true;
    }

    if (ch == KEY_RIGHT) {
        const auto& current =
            app.add.buffers[static_cast<std::size_t>(app.add.index)];
        if (app.add.cursor < static_cast<int>(current.size()))
            app.add.cursor += 1;
        return true;
    }

    if (ch == BACKSPACE_1 || ch == BACKSPACE_2 || ch == BACKSPACE_3) {
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        auto& current =
            app.add.buffers[static_cast<std::size_t>(app.add.index)];
        if (app.add.cursor > 0 && !current.empty()) {
            current.erase(static_cast<std::size_t>(app.add.cursor - 1), 1);
            app.add.cursor -= 1;
            normalize_field_buffer(key, current, &app.add.cursor);
            if (key == FieldKey::Ticker) return sync_add_type_lock_from_ticker(app);
        }
        return true;
    }

    if (ch == KEY_DC) {
        const auto key = fields[static_cast<std::size_t>(app.add.index)].key;
        auto& current =
            app.add.buffers[static_cast<std::size_t>(app.add.index)];
        if (app.add.cursor >= 0 &&
            app.add.cursor < static_cast<int>(current.size())) {
            current.erase(static_cast<std::size_t>(app.add.cursor), 1);
            normalize_field_buffer(key, current, &app.add.cursor);
            if (key == FieldKey::Ticker) return sync_add_type_lock_from_ticker(app);
        }
        return true;
    }

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        std::vector<AddState::OptValue> parsed;
        int invalid_index = -1;

        if (!validate_add_form(
                fields, app.add.buffers, &parsed, &invalid_index)) {
            app.add.index = std::max(0, invalid_index);
            auto& invalid_buf =
                app.add.buffers[static_cast<std::size_t>(app.add.index)];
            invalid_buf.clear();
            app.add.cursor = 0;
            flash_add_invalid_marker(app, input_x);
            return true;
        }

        app.add.values = std::move(parsed);
        app.add.confirming = true;
        return true;
    }

    auto& current = app.add.buffers[static_cast<std::size_t>(app.add.index)];
    const auto key = fields[static_cast<std::size_t>(app.add.index)].key;

    if (is_allowed_char_for_current_field(ch, key, current, app.add.cursor)) {
        current.insert(
            static_cast<std::size_t>(app.add.cursor), 1, static_cast<char>(ch));
        app.add.cursor += 1;
        normalize_field_buffer(key, current, &app.add.cursor);
        if (key == FieldKey::Ticker) return sync_add_type_lock_from_ticker(app);
        return true;
    }

    // swallow disallowed chars
    return true;
}

} // namespace views
