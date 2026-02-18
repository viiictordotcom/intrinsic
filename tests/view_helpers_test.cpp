#include "test_harness.hpp"
#include "views/view_add.hpp"
#include "views/view_home.hpp"
#include "views/view_ticker.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("view_add sanitize_ticker normalizes and bounds ticker text")
{
    REQUIRE_EQ(views::sanitize_ticker(" msft "), std::string("MSFT"));
    REQUIRE_EQ(views::sanitize_ticker("aapl..x$%1234567890zz"),
               std::string("AAPL.X123456"));
    REQUIRE_EQ(views::sanitize_ticker("...."), std::string("."));
}

TEST_CASE("view_add sanitize_ticker tolerates invalid bytes and very long text")
{
    std::string raw = "ab";
    raw.push_back('\0');
    raw.push_back(static_cast<char>(0xFF));
    raw.push_back('.');
    raw.append(1000, 'x');

    const std::string cleaned = views::sanitize_ticker(raw);
    REQUIRE_EQ(cleaned, std::string("AB.XXXXXXXXX"));
    REQUIRE_EQ(cleaned.size(), views::kAddTickerMaxLen);
}

TEST_CASE("view_add period parser validates accepted period formats")
{
    REQUIRE(views::period_ok("2024-Y"));
    REQUIRE(views::period_ok("2024-q1"));
    REQUIRE(views::period_ok("2024-s2"));

    REQUIRE(!views::period_ok("24-Q1"));
    REQUIRE(!views::period_ok("2024-Q5"));
    REQUIRE(!views::period_ok("2024-"));
}

TEST_CASE("view_add numeric parsers reject malformed and overflow inputs")
{
    std::int64_t as_i64 = 0;
    REQUIRE(views::parse_int64(" -42 ", &as_i64));
    REQUIRE_EQ(as_i64, std::int64_t{-42});
    REQUIRE(!views::parse_int64("12.3", &as_i64));
    REQUIRE(!views::parse_int64("9223372036854775808", &as_i64));

    double as_f64 = 0.0;
    REQUIRE(views::parse_double(" 3.14 ", &as_f64));
    REQUIRE_EQ(as_f64, 3.14);
    REQUIRE(!views::parse_double("3.14x", &as_f64));
}

TEST_CASE("view_add validate_and_parse enforces value ranges and types")
{
    AddState::OptValue value;

    REQUIRE(
        views::validate_and_parse(" msft ", views::FieldKey::Ticker, &value));
    REQUIRE(std::holds_alternative<std::string>(*value));
    REQUIRE_EQ(std::get<std::string>(*value), std::string("MSFT"));

    REQUIRE(
        views::validate_and_parse("2024-q4", views::FieldKey::Period, &value));
    REQUIRE_EQ(std::get<std::string>(*value), std::string("2024-Q4"));

    REQUIRE(views::validate_and_parse("", views::FieldKey::Revenue, &value));
    REQUIRE(!value.has_value());

    REQUIRE(!views::validate_and_parse("-1", views::FieldKey::Revenue, &value));
    REQUIRE(
        views::validate_and_parse("-1", views::FieldKey::NetIncome, &value));
    REQUIRE(std::holds_alternative<std::int64_t>(*value));

    REQUIRE(
        !views::validate_and_parse("1000000", views::FieldKey::Eps, &value));
}

TEST_CASE("view_add character admission blocks invalid input patterns")
{
    REQUIRE(views::is_allowed_char_for_current_field(
        'A', views::FieldKey::Ticker, "", 0));
    REQUIRE(!views::is_allowed_char_for_current_field(
        '@', views::FieldKey::Ticker, "", 0));

    REQUIRE(!views::is_allowed_char_for_current_field(
        '-', views::FieldKey::Revenue, "", 0));
    REQUIRE(views::is_allowed_char_for_current_field(
        '-', views::FieldKey::NetIncome, "", 0));

    REQUIRE(views::is_allowed_char_for_current_field(
        '.', views::FieldKey::Eps, "1", 1));
    REQUIRE(!views::is_allowed_char_for_current_field(
        '.', views::FieldKey::Eps, "1.0", 3));
}

TEST_CASE("view_home index helpers clamp and fallback predictably")
{
    REQUIRE_EQ(views::home_index_for_cell(7, 0, 0, 3, 5), 0);
    REQUIRE_EQ(views::home_index_for_cell(7, 1, 1, 3, 5), 6);
    REQUIRE_EQ(views::home_index_for_cell(7, 1, 4, 3, 5), -1);
    REQUIRE_EQ(views::home_index_for_cell(0, 0, 0, 3, 5), -1);

    REQUIRE_EQ(views::home_best_index_in_col(7, 1, 4, 3, 5), 6);
    REQUIRE_EQ(views::home_best_index_in_col(2, 2, 1, 3, 5), -1);
}

TEST_CASE("view_ticker parsing and numeric guards handle invalid values")
{
    REQUIRE_EQ(views::format_i64_value(999), std::string("999"));
    REQUIRE_EQ(views::format_i64_value(1234), std::string("1K"));
    REQUIRE_EQ(views::format_i64_value(3000000), std::string("3M"));
    REQUIRE_EQ(views::format_i64_value(1000000000000LL), std::string("1T"));
    REQUIRE_EQ(views::format_i64_value(-2000000000000LL), std::string("-2T"));
    REQUIRE_EQ(views::format_i64_value(-1234), std::string("-1K"));
    REQUIRE_EQ(views::format_i64_value(-3000000), std::string("-3M"));
    REQUIRE_EQ(views::format_compact_i64_from_f64_opt(
                   std::optional<double>{-20000000.0}),
               std::string("-20M"));
    REQUIRE_EQ(views::format_compact_i64_from_f64_opt(
                   std::optional<double>{-2000000000000.0}),
               std::string("-2T"));

    REQUIRE_EQ(views::parse_decimal_input("12.5"), std::optional<double>{12.5});
    REQUIRE(!views::parse_decimal_input("12.5x").has_value());
    REQUIRE(!views::parse_decimal_input("1e309").has_value());
    REQUIRE(!views::null_if_zero_or_invalid(
                 std::optional<double>{std::numeric_limits<double>::infinity()})
                 .has_value());
    REQUIRE(
        !views::null_if_zero_or_invalid(
             std::optional<double>{std::numeric_limits<double>::quiet_NaN()})
             .has_value());
    REQUIRE_EQ(views::format_f64_integer_opt(std::optional<double>{1e300}),
               std::string(views::kNaValue));
    REQUIRE_EQ(views::format_f64_integer_opt(std::optional<double>{-1e300}),
               std::string(views::kNaValue));

    REQUIRE(!views::div_opt_nonzero(std::optional<double>{10.0},
                                    std::optional<double>{0.0})
                 .has_value());
    REQUIRE(!views::null_if_negative(std::optional<double>{-1.0}).has_value());
}

TEST_CASE("view_ticker TTM helper computes rolling sums and handles gaps")
{
    auto row =
        [](int year, const char* period, std::optional<std::int64_t> revenue) {
            db::Database::FinanceRow r{};
            r.year = year;
            r.period_type = period;
            r.revenue = revenue;
            return r;
        };

    const std::vector<db::Database::FinanceRow> rows = {
        row(2023, "Q4", 100),
        row(2024, "Q1", 110),
        row(2024, "Q2", 120),
        row(2024, "Q3", 130),
    };

    const auto sum = views::ttm_sum_for_family(
        rows, 3, 'Q', 4, [](const db::Database::FinanceRow& r) {
            return views::to_f64(r.revenue);
        });
    REQUIRE_EQ(sum, std::optional<double>{460.0});

    const auto missing = views::ttm_sum_for_family(
        rows, 3, 'Q', 5, [](const db::Database::FinanceRow& r) {
            return views::to_f64(r.revenue);
        });
    REQUIRE(!missing.has_value());

    auto with_null = rows;
    with_null[2].revenue = std::nullopt;
    const auto invalid = views::ttm_sum_for_family(
        with_null, 3, 'Q', 4, [](const db::Database::FinanceRow& r) {
            return views::to_f64(r.revenue);
        });
    REQUIRE(!invalid.has_value());
}

TEST_CASE("view_ticker change-format helpers parse and colorize consistently")
{
    REQUIRE_EQ(views::percent_change(std::optional<double>{120.0},
                                     std::optional<double>{100.0}),
               std::optional<double>{20.0});
    REQUIRE(!views::percent_change(std::optional<double>{100.0},
                                   std::optional<double>{0.0})
                 .has_value());

    REQUIRE_EQ(views::format_change(std::optional<double>{1500.0}),
               std::string("2k%"));
    REQUIRE_EQ(views::format_change(std::optional<double>{-2500.0}),
               std::string("-3k%"));

    std::string value;
    std::string change;
    REQUIRE(views::split_value_and_change("123 10.0%", &value, &change));
    REQUIRE_EQ(value, std::string("123"));
    REQUIRE_EQ(change, std::string("10.0%"));
    REQUIRE(!views::split_value_and_change("123", &value, &change));

    REQUIRE_EQ(views::color_pair_for_change_text("10.0%"),
               views::kColorPairPositive);
    REQUIRE_EQ(views::color_pair_for_change_text("-10.0%"),
               views::kColorPairNegative);
    REQUIRE_EQ(views::color_pair_for_change_text("10.0%", true),
               views::kColorPairNegative);
}

TEST_CASE("view_ticker input guard enforces one dot and max length")
{
    std::string buf = "12.3";
    REQUIRE(!views::is_allowed_ticker_input_char('.', buf));
    REQUIRE(views::is_allowed_ticker_input_char('4', buf));

    std::string full(views::kTickerInputMaxLen, '1');
    REQUIRE(!views::is_allowed_ticker_input_char('2', full));
    REQUIRE(!views::is_allowed_ticker_input_char(-1, ""));
    REQUIRE(!views::is_allowed_ticker_input_char(300, ""));
}

TEST_CASE("view_ticker input-dependent overflow guard is length-based")
{
    REQUIRE(!views::input_metric_overflows_width("123.45", "", 8, false));
    REQUIRE(
        views::input_metric_overflows_width("-123456789012345", "", 8, false));
    REQUIRE(views::input_metric_overflows_width(
        "123456789012345.12345", "", 10, false));

    REQUIRE(!views::input_metric_overflows_width("123", "10.0%", 10, true));
    REQUIRE(views::input_metric_overflows_width("1234", "10.0%", 9, true));
    REQUIRE(views::input_metric_overflows_width(
        "-9999999", "-1234567k%", 18, true));
}


