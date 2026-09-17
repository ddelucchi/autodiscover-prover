/**
 * @file Budget.hpp
 * @brief Shared budget / gas system for bounding all loops
 * 
 * =============================================================================
 * PURPOSE
 * =============================================================================
 * 
 * Every long-running sub-system in AutoDiscoverProver (e-graph rebuild,
 * saturation, inference, normalization, orbit computation, proof search)
 * shares a single Budget object.  This provides a uniform mechanism for:
 * 
 *   1. Wall-clock deadlines  (absolute time point)
 *   2. Step/gas counters     (decremented on each unit of work)
 *   3. Composability         (sub-budgets for nested loops)
 * 
 * Any loop that could diverge should call budget.tick() on each iteration
 * and exit early when tick() returns false.
 * 
 * =============================================================================
 * USAGE
 * =============================================================================
 * 
 *   Budget budget(100'000, std::chrono::seconds(30));  // 100k steps, 30s
 *   while (budget.tick()) {
 *       // ... do one unit of work ...
 *   }
 *   if (budget.exhausted()) { ... handle timeout/gas-out ... }
 *
 *   // Sub-budget for a nested loop (inherits deadline, independent gas):
 *   Budget inner = budget.sub(5000);
 *   while (inner.tick()) { ... }
 */

#ifndef AUTODISCOVER_CORE_BUDGET_HPP
#define AUTODISCOVER_CORE_BUDGET_HPP

#include <cstdint>
#include <chrono>
#include <limits>
#include <algorithm>

namespace autodiscover {
namespace core {

class Budget {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    
    /**
     * @brief Construct a budget with step limit and/or wall-clock deadline.
     * 
     * @param maxSteps  Maximum number of tick() calls.  0 = unlimited.
     * @param timeout   Maximum wall-clock duration.  0 = no deadline.
     */
    explicit Budget(uint64_t maxSteps = 0,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds(0))
        : remaining_(maxSteps == 0 ? std::numeric_limits<uint64_t>::max() : maxSteps)
        , totalSteps_(maxSteps)
        , consumed_(0)
    {
        if (timeout.count() > 0) {
            deadline_ = Clock::now() + timeout;
            hasDeadline_ = true;
        }
    }
    
    /**
     * @brief Construct with an absolute deadline (for sub-budgets).
     */
    Budget(uint64_t maxSteps, TimePoint deadline)
        : remaining_(maxSteps == 0 ? std::numeric_limits<uint64_t>::max() : maxSteps)
        , totalSteps_(maxSteps)
        , consumed_(0)
        , deadline_(deadline)
        , hasDeadline_(true)
    {}
    
    /**
     * @brief Consume one unit of gas and check limits.
     * 
     * @return true  if work may continue
     * @return false if budget is exhausted (gas or time)
     */
    [[nodiscard]] bool tick() {
        if (remaining_ == 0) return false;
        --remaining_;
        ++consumed_;
        
        // Check wall-clock every 256 ticks to amortise the syscall
        if (hasDeadline_ && (consumed_ & 0xFF) == 0) {
            if (Clock::now() >= deadline_) {
                remaining_ = 0;
                return false;
            }
        }
        return true;
    }
    
    /**
     * @brief Consume N units of gas at once.
     * @return true if budget is NOT yet exhausted after consuming N.
     */
    [[nodiscard]] bool consume(uint64_t n) {
        if (n >= remaining_) { remaining_ = 0; return false; }
        remaining_ -= n;
        consumed_ += n;
        return true;
    }
    
    /**
     * @brief Check whether the budget is exhausted.
     */
    [[nodiscard]] bool exhausted() const {
        if (remaining_ == 0) return true;
        if (hasDeadline_ && Clock::now() >= deadline_) return true;
        return false;
    }
    
    /**
     * @brief Remaining gas.
     */
    [[nodiscard]] uint64_t remaining() const { return remaining_; }
    
    /**
     * @brief Steps consumed so far.
     */
    [[nodiscard]] uint64_t consumed() const { return consumed_; }
    
    /**
     * @brief Create a sub-budget that inherits this budget's deadline
     *        but has its own independent gas counter.
     * 
     * @param maxSteps  Step limit for the sub-budget (0 = gas from parent).
     */
    [[nodiscard]] Budget sub(uint64_t maxSteps) const {
        uint64_t effectiveSteps = maxSteps == 0 ? remaining_ : std::min(maxSteps, remaining_);
        if (hasDeadline_) {
            return Budget(effectiveSteps, deadline_);
        }
        return Budget(effectiveSteps, std::chrono::milliseconds(0));
    }
    
    /**
     * @brief Deduct consumed sub-budget gas from this (parent) budget.
     * 
     * Call after a sub-budget finishes so the parent's remaining gas
     * reflects the work done in the child.
     */
    void deduct(const Budget& child) {
        uint64_t used = child.consumed();
        if (used >= remaining_) {
            remaining_ = 0;
        } else {
            remaining_ -= used;
        }
        consumed_ += used;
    }
    
    /**
     * @brief Check if this budget has a deadline.
     */
    [[nodiscard]] bool hasDeadline() const { return hasDeadline_; }
    
    /**
     * @brief Get the deadline time point (only valid if hasDeadline()).
     */
    [[nodiscard]] TimePoint deadline() const { return deadline_; }
    
    /**
     * @brief Unlimited budget (no gas limit, no deadline).
     */
    static Budget unlimited() { return Budget(); }

private:
    uint64_t remaining_;
    uint64_t totalSteps_;
    uint64_t consumed_;
    TimePoint deadline_ = TimePoint::max();
    bool hasDeadline_ = false;
};

} // namespace core
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_BUDGET_HPP
