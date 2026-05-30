// test_framework.hpp - a ~50-line, dependency-free test harness.
//
// Tests self-register via the TEST() macro; test/test_main.cpp runs them all.
// No gtest/catch needed, so the unit suite builds with just a C++ compiler
// (and crucially, with no OpenCV) on any CI runner.
#pragma once
#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace tf {

struct Failure { std::string msg; };

struct TestCase { std::string name; std::function<void()> fn; };

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int runAll() {
    int failed = 0;
    for (auto& t : registry()) {
        try {
            t.fn();
            std::cout << "[ ok ] " << t.name << "\n";
        } catch (const Failure& f) {
            std::cout << "[FAIL] " << t.name << "\n        " << f.msg << "\n";
            ++failed;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << "\n        exception: "
                      << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << "\n" << (registry().size() - failed) << "/" << registry().size()
              << " tests passed\n";
    return failed == 0 ? 0 : 1;
}

} // namespace tf

#define TEST(suite, name)                                                     \
    static void suite##_##name##_impl();                                      \
    static ::tf::Registrar suite##_##name##_reg(                              \
        #suite "." #name, suite##_##name##_impl);                             \
    static void suite##_##name##_impl()

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::ostringstream _os;                                           \
            _os << __FILE__ << ":" << __LINE__ << "  CHECK(" #cond ")";       \
            throw ::tf::Failure{_os.str()};                                   \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        auto _a = (a);                                                        \
        auto _b = (b);                                                        \
        if (!(_a == _b)) {                                                    \
            std::ostringstream _os;                                           \
            _os << __FILE__ << ":" << __LINE__ << "  CHECK_EQ(" #a ", " #b    \
                ") got " << +_a << " vs " << +_b;                             \
            throw ::tf::Failure{_os.str()};                                   \
        }                                                                     \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                 \
    do {                                                                      \
        double _d = std::fabs((double)(a) - (double)(b));                     \
        if (_d > (eps)) {                                                     \
            std::ostringstream _os;                                           \
            _os << __FILE__ << ":" << __LINE__ << "  CHECK_NEAR(" #a ", " #b  \
                ") diff " << _d << " > " << (eps);                            \
            throw ::tf::Failure{_os.str()};                                   \
        }                                                                     \
    } while (0)
