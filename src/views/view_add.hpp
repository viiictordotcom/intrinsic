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
        return {ValueKind::Int64, 0.0, 1e14};
    case FieldKey::NetIncome:
    case FieldKey::CfoOperations:
    case FieldKey::CfiInvesting:
    case FieldKey::CffFinancing:
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

inline const std::vector<AddField>& add_fields()
{
    static const std::vector<AddField> f = {
        {FieldKey::Ticker, "ticker"},
        {FieldKey::Period, "period"},

        {FieldKey::CashAndEquivalents, "cash and equivalents"},
        {FieldKey::CurrentAssets, "current assets"},
        {FieldKey::NonCurrentAssets, "non-current assets"},
        {FieldKey::CurrentLiabilities, "current liabilities"},
        {FieldKey::NonCurrentLiabilities, "non-current liabilities"},

        {FieldKey::Revenue, "revenue"},
        {FieldKey::NetIncome, "net income"},
        {FieldKey::Eps, "eps"},

        {FieldKey::CfoOperations, "operations"},
        {FieldKey::CfiInvesting, "investing"},
        {FieldKey::CffFinancing, "financing"},
    };

    return f;
}

inline int add_input_x()
{
    static const int x = [] {
        const auto& fields = add_fields();

        std::size_t max_len = 0;
        for (const auto& f : fields) {
            if (f.label) max_len = std::max(max_len, std::strlen(f.label));
        }

        // start of labels + longest label + cushion
        return kAddInputTab + static_cast<int>(max_len) + kAddInputCushion;
    }();
    return x;
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

inline void open_add_create(AppState& app)
{
    app.add.reset(static_cast<int>(add_fields().size()));
    app.add.mode = AddMode::Create;
    app.current = views::ViewId::Add;
}

inline void open_add_prefilled_from_ticker(AppState& app,
                                           const db::Database::FinanceRow& row)
{
    app.add.reset(static_cast<int>(add_fields().size()));
    // Edit mode reuses the add form with fields prefilled from the selected
    // period.
    app.add.mode = AddMode::EditFromTicker;

    app.add.buffers[0] = row.ticker;
    app.add.buffers[1] = add_period_label(row);
    app.add.buffers[2] = opt_i64_to_input(row.cash_and_equivalents);
    app.add.buffers[3] = opt_i64_to_input(row.current_assets);
    app.add.buffers[4] = opt_i64_to_input(row.non_current_assets);
    app.add.buffers[5] = opt_i64_to_input(row.current_liabilities);
    app.add.buffers[6] = opt_i64_to_input(row.non_current_liabilities);
    app.add.buffers[7] = opt_i64_to_input(row.revenue);
    app.add.buffers[8] = opt_i64_to_input(row.net_income);
    app.add.buffers[9] = opt_f64_to_input(row.eps);
    app.add.buffers[10] = opt_i64_to_input(row.cash_flow_from_operations);
    app.add.buffers[11] = opt_i64_to_input(row.cash_flow_from_investing);
    app.add.buffers[12] = opt_i64_to_input(row.cash_flow_from_financing);

    app.current = views::ViewId::Add;
}

inline void ensure_add_initialized(AppState& app)
{
    const int field_count = static_cast<int>(add_fields().size());
    if (!app.add.active) {
        app.add.reset(field_count);
        return;
    }

    if (static_cast<int>(app.add.buffers.size()) != field_count ||
        static_cast<int>(app.add.values.size()) != field_count ||
        static_cast<int>(app.add.layout_y.size()) != field_count) {
        app.add.reset(field_count);
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

    const int input_x = add_input_x();
    const auto& fields = add_fields();
    clamp_add_index(app, static_cast<int>(fields.size()));

    int y = 0;
    y += 1; // title
    y += 1; // spacer

    std::vector<int> logical_field_y(fields.size(), 0);
    for (int i = 0; i < (int)fields.size(); ++i) {
        if (i == 2 || i == 7 || i == 10) y += 2; // spacer + section title
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
        const char* suffix =
            (app.add.mode == AddMode::EditFromTicker) ? " edit" : " add";
        if (has_colors()) attron(COLOR_PAIR(3));
        attron(A_BOLD);
        mvprintw(screen_y, 0, "intrinsic ~");
        attroff(A_BOLD);
        if (has_colors()) attroff(COLOR_PAIR(3));
        if (COLS > 11) mvprintw(screen_y, 11, "%s", suffix);
    }
    logical_y += 2;

    const int label_w = std::max(1, input_x - kAddInputTab - kAddInputCushion);

    for (int i = 0; i < (int)fields.size(); ++i) {
        if (i == 2 || i == 7 || i == 10) {
            logical_y += 1;
            screen_y = to_screen_y(logical_y);
            if (screen_y >= 0 && screen_y < viewport) {
                const char* title = (i == 2)   ? "BALANCE"
                                    : (i == 7) ? "INCOME"
                                               : "CASH FLOW";
                mvprintw(screen_y, 0, "%s", title);
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
        const int cursor_x = input_x + app.add.cursor;
        if (line_y >= 0 && line_y < viewport) {
            move(line_y, std::min(cursor_x, std::max(0, COLS - 1)));
        }
        curs_set(1);
    }

    if (total_lines > LINES && LINES > 0) {
        mvprintw(LINES - 1,
                 0,
                 "auto-scroll (%d/%d)",
                 app.add.scroll + 1,
                 max_scroll + 1);
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

inline bool handle_key_add(AppState& app, int ch)
{
    ensure_add_initialized(app);

    const int input_x = add_input_x();
    const auto& fields = add_fields();
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
            auto ticker_opt = as_str_opt(app.add.values[0]);
            auto period_opt = as_str_opt(app.add.values[1]);
            if (!ticker_opt || !period_opt) {
                route_error(app, "ticker/period missing");
                return true;
            }

            db::Database::FinancePayload payload{};
            payload.cash_and_equivalents = as_i64_opt(app.add.values[2]);
            payload.current_assets = as_i64_opt(app.add.values[3]);
            payload.non_current_assets = as_i64_opt(app.add.values[4]);
            payload.current_liabilities = as_i64_opt(app.add.values[5]);
            payload.non_current_liabilities = as_i64_opt(app.add.values[6]);
            payload.revenue = as_i64_opt(app.add.values[7]);
            payload.net_income = as_i64_opt(app.add.values[8]);
            payload.eps = as_f64_opt(app.add.values[9]);
            payload.cash_flow_from_operations = as_i64_opt(app.add.values[10]);
            payload.cash_flow_from_investing = as_i64_opt(app.add.values[11]);
            payload.cash_flow_from_financing = as_i64_opt(app.add.values[12]);

            std::string err;
            if (!app.db->add_finances(
                    *ticker_opt, *period_opt, payload, &err)) {
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

                app.ticker_view.reset(*ticker_opt, std::move(refreshed));
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
        auto& current =
            app.add.buffers[static_cast<std::size_t>(app.add.index)];
        if (app.add.cursor > 0 && !current.empty()) {
            current.erase(static_cast<std::size_t>(app.add.cursor - 1), 1);
            app.add.cursor -= 1;
            normalize_field_buffer(
                fields[static_cast<std::size_t>(app.add.index)].key,
                current,
                &app.add.cursor);
        }
        return true;
    }

    if (ch == KEY_DC) {
        auto& current =
            app.add.buffers[static_cast<std::size_t>(app.add.index)];
        if (app.add.cursor >= 0 &&
            app.add.cursor < static_cast<int>(current.size())) {
            current.erase(static_cast<std::size_t>(app.add.cursor), 1);
            normalize_field_buffer(
                fields[static_cast<std::size_t>(app.add.index)].key,
                current,
                &app.add.cursor);
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
        return true;
    }

    // swallow disallowed chars
    return true;
}

} // namespace views

