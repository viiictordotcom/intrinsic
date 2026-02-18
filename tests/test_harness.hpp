#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace test {

class Failure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, void (*fn)())
    {
        registry().push_back({name, fn});
    }
};

[[noreturn]] inline void fail(const char* expr, const char* file, int line)
{
    std::ostringstream oss;
    oss << file << ':' << line << ": requirement failed: " << expr;
    throw Failure(oss.str());
}

[[noreturn]] inline void
fail_with_message(const std::string& message, const char* file, int line)
{
    std::ostringstream oss;
    oss << file << ':' << line << ": " << message;
    throw Failure(oss.str());
}

template <typename A, typename B>
inline void require_eq(const A& actual,
                       const B& expected,
                       const char* actual_expr,
                       const char* expected_expr,
                       const char* file,
                       int line)
{
    if (actual == expected) return;

    std::ostringstream oss;
    oss << "equality check failed: " << actual_expr << " != " << expected_expr;
    fail_with_message(oss.str(), file, line);
}

inline void require_contains(const std::string& haystack,
                             const std::string& needle,
                             const char* haystack_expr,
                             const char* needle_expr,
                             const char* file,
                             int line)
{
    if (haystack.find(needle) != std::string::npos) return;

    std::ostringstream oss;
    oss << "substring check failed: " << haystack_expr << " does not contain "
        << needle_expr;
    fail_with_message(oss.str(), file, line);
}

} // namespace test

#define TEST_CONCAT_INNER_(a, b) a##b
#define TEST_CONCAT_(a, b) TEST_CONCAT_INNER_(a, b)

#define TEST_CASE(name)                                                        \
    static void TEST_CONCAT_(test_case_fn_, __LINE__)();                       \
    static ::test::Registrar TEST_CONCAT_(test_case_registrar_, __LINE__)(     \
        name, &TEST_CONCAT_(test_case_fn_, __LINE__));                         \
    static void TEST_CONCAT_(test_case_fn_, __LINE__)()

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        if (!(expr)) ::test::fail(#expr, __FILE__, __LINE__);                  \
    } while (false)

#define REQUIRE_EQ(actual, expected)                                           \
    do {                                                                       \
        ::test::require_eq(                                                    \
            (actual), (expected), #actual, #expected, __FILE__, __LINE__);     \
    } while (false)

#define REQUIRE_CONTAINS(haystack, needle)                                     \
    do {                                                                       \
        ::test::require_contains(                                              \
            (haystack), (needle), #haystack, #needle, __FILE__, __LINE__);     \
    } while (false)

#define REQUIRE_THROWS(stmt)                                                   \
    do {                                                                       \
        bool threw_ = false;                                                   \
        try {                                                                  \
            (void)(stmt);                                                      \
        }                                                                      \
        catch (...) {                                                          \
            threw_ = true;                                                     \
        }                                                                      \
        if (!threw_) {                                                         \
            ::test::fail("expected exception: " #stmt, __FILE__, __LINE__);    \
        }                                                                      \
    } while (false)


