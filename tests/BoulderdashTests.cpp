#include <exception>
#include <iostream>
#include <vector>

#include "TestSuites.h"
#include "util/Logger.h"

int main() {
    Logger::setEnabled(false);

    std::vector<TestCase> tests;
    auto append = [&](const std::vector<TestCase>& suite) {
        tests.insert(tests.end(), suite.begin(), suite.end());
    };
    append(gameTests());
    append(systemTests());
    append(dataTests());

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    std::cout << tests.size() - static_cast<std::size_t>(failures) << "/"
              << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
