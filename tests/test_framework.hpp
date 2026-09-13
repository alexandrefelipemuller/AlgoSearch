#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

struct AssertionFailure {
    std::string message;
};

inline int& failureCountRef() {
    static int c = 0;
    return c;
}

}  // namespace testfw

#define TEST(name)                                                                        \
    void test_##name();                                                                   \
    static testfw::Registrar registrar_##name(#name, test_##name);                         \
    void test_##name()

#define CHECK(cond)                                                                        \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::ostringstream oss;                                                         \
            oss << "CHECK failed: " #cond " at " << __FILE__ << ":" << __LINE__;            \
            throw testfw::AssertionFailure{oss.str()};                                      \
        }                                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                                      \
    do {                                                                                    \
        if (!((a) == (b))) {                                                                \
            std::ostringstream oss;                                                         \
            oss << "CHECK_EQ failed: " #a " == " #b " at " << __FILE__ << ":" << __LINE__;  \
            throw testfw::AssertionFailure{oss.str()};                                      \
        }                                                                                    \
    } while (0)
