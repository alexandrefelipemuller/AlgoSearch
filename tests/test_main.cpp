#include <iostream>

#include "test_framework.hpp"

int main() {
    int passed = 0, failed = 0;
    for (const auto& tc : testfw::registry()) {
        try {
            tc.fn();
            std::cout << "[PASS] " << tc.name << "\n";
            passed++;
        } catch (const testfw::AssertionFailure& e) {
            std::cout << "[FAIL] " << tc.name << " - " << e.message << "\n";
            failed++;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << tc.name << " - unexpected exception: " << e.what() << "\n";
            failed++;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed, " << (passed + failed) << " total\n";
    return failed == 0 ? 0 : 1;
}
