#pragma once

#include <cstdint>

namespace DebugBudget {

constexpr std::uint64_t kStepCommitCycleBudget = 4096;
constexpr std::uint64_t kInteractiveOsBootMaxSteps = 5000000;
constexpr std::uint64_t kInteractiveOsCommandMaxSteps = 500000;
constexpr std::uint64_t kCourseOsShellBootMaxSteps = 5000000;
constexpr std::uint64_t kCourseOsShellCommandMaxSteps = 1000000;

}  // namespace DebugBudget
