/**
 * @file bdmergememorybudget.h
 * @brief Shared pressure-aware budget for decoded texture and mesh pools.
 */

#ifndef BDMERGE_MEMORY_BUDGET_H
#define BDMERGE_MEMORY_BUDGET_H

#include <iosfwd>

class BDMergeMemoryBudget
{
public:
    // Main thread only. Samples at most once per second and configures both
    // decoded caches from one live physical/commit/process budget.
    static void refresh();

    // Thread-safe. Critical pressure temporarily overrides capture pinning.
    static bool isCritical();

    // Thread-safe diagnostic snapshot.
    static void appendReport(std::ostream& out);
};

#endif // BDMERGE_MEMORY_BUDGET_H
