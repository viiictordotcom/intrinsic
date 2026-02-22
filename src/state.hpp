#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <array>
#include <cstdint>

#include "db/database.hpp"
#include "views/view.hpp"

enum class AddMode {
    Create,
    EditFromTicker,
};

struct AddState {
    using Value = std::variant<std::int64_t, double, std::string>;
    using OptValue = std::optional<Value>;

    // transient state backing the add/edit overlay form
    bool active = false;
    AddMode mode = AddMode::Create;
    int index = 0;
    int scroll = 0;
    int cursor = 0;
    std::vector<std::string> buffers;
    std::vector<OptValue> values;
    bool confirming = false;

    // layout cache -> render_add fills
    std::vector<int> layout_y;

    void reset(int field_count)
    {
        active = true;
        mode = AddMode::Create;
        index = 0;
        scroll = 0;
        cursor = 0;
        buffers.assign(field_count, std::string{});
        values.assign(field_count, std::nullopt);
        layout_y.assign(field_count, 0);
        confirming = false;
    }
};

struct AppState {
    db::Database* db = nullptr;                  // non-owning dep-injection
    views::ViewId current = views::ViewId::Home; // current view
    std::string last_error;                      // error view message bus
    bool quit_requested = false;                 // request clean main-loop exit

    AddState add;

    struct Settings {
        // defaults
        db::Database::TickerSortKey sort_key =
            db::Database::TickerSortKey::LastUpdate;
        db::Database::SortDir sort_dir = db::Database::SortDir::Desc;
        bool ttm = false;
        bool show_help = true;
    } settings;

    struct SettingsViewState {
        bool nuke_confirm_armed = false;
        bool update_confirm_armed = false;
        // one-line feedback for update/check actions
        std::string update_status_line;
    } settings_view;

    struct TickerListState {
        int page = 0;
        int page_size = 15;
        int selected = 0;
        int row_scroll = 0;
        bool search_mode = false;
        bool search_exit_armed = false;
        bool portfolio_only = false;
        std::string search_query;
        // frozen query used for the currently displayed results
        std::string search_submitted_query;
        std::vector<db::Database::TickerRow> search_rows;
        std::vector<db::Database::TickerRow> last_rows;

        struct Prefetch {
            int page = 0;
            int page_size = 0;
            db::Database::TickerSortKey sort_key{};
            db::Database::SortDir sort_dir{};
            bool portfolio_only = false;
            std::vector<db::Database::TickerRow> rows;
            bool valid = false;
        } prefetch;

        void invalidate_prefetch() { prefetch.valid = false; }

        void clear_search()
        {
            search_mode = false;
            search_exit_armed = false;
            search_query.clear();
            search_submitted_query.clear();
            search_rows.clear();
            row_scroll = 0;
        }
    } tickers;

    struct TickerViewState {
        std::string ticker;
        std::vector<db::Database::FinanceRow> all_rows;
        std::vector<db::Database::FinanceRow> rows;
        int index = 0;
        int scroll = 0;
        std::string status_line;
        bool yearly_only = false;
        // inline editor buffers for date/value inputs
        std::array<std::string, 2> inputs{};
        int input_index = 0;

        void reset(std::string next_ticker,
                   std::vector<db::Database::FinanceRow> next_rows)
        {
            ticker = std::move(next_ticker);
            all_rows = std::move(next_rows);
            rows = all_rows;
            index = rows.empty() ? 0 : static_cast<int>(rows.size() - 1);
            scroll = 0;
            status_line.clear();
            yearly_only = false;
            inputs = {};
            input_index = 0;
        }

        void clamp_index()
        {
            if (rows.empty()) {
                index = 0;
                return;
            }
            if (index < 0) index = 0;
            if (index >= static_cast<int>(rows.size()))
                index = static_cast<int>(rows.size() - 1);
        }
    } ticker_view;
};

// *
// **
// ***
// ****
// ***** HELPERS

inline void route_error(AppState& app, std::string err)
{
    app.last_error = err.empty() ? "Unknown error" : std::move(err);
    app.current = views::ViewId::Error;
}

inline void route_error(AppState& app, const char* err)
{
    route_error(app, std::string(err ? err : ""));
}
