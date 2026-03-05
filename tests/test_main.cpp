#include "test_harness.hpp"

#include <exception>
#include <iostream>

int main()
{
    const auto& tests = test::registry();

    int passed = 0;
    int failed = 0;

    for (const auto& t : tests) {
        try {
            t.fn();
            std::cout << "[PASS] " << t.name << '\n';
            ++passed;
        }
        catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << " :: " << e.what() << '\n';
            ++failed;
        }
        catch (...) {
            std::cout << "[FAIL] " << t.name << " :: unknown exception\n";
            ++failed;
        }
    }

    std::cout << "\nSummary: " << passed << " passed, " << failed
              << " failed\n";
    return failed == 0 ? 0 : 1;
}


