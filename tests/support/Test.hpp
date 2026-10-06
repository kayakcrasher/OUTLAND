#pragma once

#include <cmath>
#include <stdexcept>
#include <string>

namespace outland::test {

inline void require(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void require_near(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
) {
    if (std::fabs(actual - expected) > tolerance) {
        throw std::runtime_error(message);
    }
}

}
