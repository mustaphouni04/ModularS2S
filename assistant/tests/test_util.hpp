#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

// Minimal assertion helper so tests don't need to pull in a test framework
// dependency. CHECK() reports and continues; a single failure fails the
// whole test binary via a nonzero exit code (checked in main via g_failures).
inline int g_failures = 0;

#define CHECK_EQ(actual, expected)                                                        \
    do {                                                                                   \
        auto a = (actual);                                                                 \
        auto e = (expected);                                                               \
        if (!(a == e)) {                                                                   \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " expected [" << e      \
                       << "] got [" << a << "]\n";                                         \
            ++g_failures;                                                                  \
        }                                                                                  \
    } while (0)

#define CHECK(cond)                                                                        \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " condition failed: " #cond \
                       << "\n";                                                             \
            ++g_failures;                                                                   \
        }                                                                                    \
    } while (0)
