// Runs all registered tests.
// Usage: atm_domain_tests              - all tests
//        atm_domain_tests V1Money      - only tests whose name contains "V1Money"
// Exit code: 0 = all passed, 1 = failures, 2 = no test matched the filter.
#include <exception>
#include <iostream>
#include <string>

#include "support/mini_test.hpp"

int main(int argc, char** argv) {
    const std::string filter = (argc > 1) ? argv[1] : "";
    int run = 0;
    int failedTests = 0;

    for (const auto& test : mini_test::registry()) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) {
            continue;
        }
        ++run;
        const int before = mini_test::failureCount();
        try {
            test.body();
        }
        catch (const std::exception& e) {
            ++mini_test::failureCount();
            std::cerr << "  FAILED unexpected exception: " << e.what() << "\n";
        }
        catch (...) {
            ++mini_test::failureCount();
            std::cerr << "  FAILED unknown exception\n";
        }
        const bool ok = (mini_test::failureCount() == before);
        if (!ok) {
            ++failedTests;
        }
        std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << test.name << "\n";
    }

    if (run == 0) {
        std::cerr << "No tests matched filter '" << filter << "'\n";
        return 2;
    }
    std::cout << "\n"
        << run << " tests run, " << (run - failedTests) << " passed, " << failedTests
        << " failed\n";
    return failedTests == 0 ? 0 : 1;
}