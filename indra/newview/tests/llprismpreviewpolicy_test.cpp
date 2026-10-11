/**
 * @file llprismpreviewpolicy_test.cpp
 * @brief Pure scheduler-policy regressions for VCam preview demand.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../llprismpreview.h"

#include <cmath>

namespace tut
{
struct prism_preview_policy_data {};
typedef test_group<prism_preview_policy_data> prism_preview_policy_group;
typedef prism_preview_policy_group::object prism_preview_policy_object;
prism_preview_policy_group prism_preview_policy_tests("LLPrismPreviewPolicy");

template<> template<>
void prism_preview_policy_object::test<1>()
{
    set_test_name("visible tier consumes capacity before preview tier");
    std::array<bool, LLPrismLens::MAX_CAPTURES> visible{};
    std::array<bool, LLPrismLens::MAX_CAPTURES> preview{};
    std::array<F32, LLPrismLens::MAX_CAPTURES> demand{};
    std::array<F32, LLPrismLens::MAX_CAPTURES> entitlement{};
    visible[0] = true;
    visible[1] = true;
    preview[2] = true;
    preview[3] = true;
    demand[0] = 4.f;
    demand[1] = 8.f;
    demand[2] = 5.f;
    demand[3] = 5.f;

    F32 remaining = LLPrismPreviewPolicy::waterFillTier(
        visible, demand, 15.f, entitlement);
    LLPrismPreviewPolicy::waterFillTier(
        preview, demand, remaining, entitlement);
    ensure("visible low-rate producer is satisfied", entitlement[0] == 4.f);
    ensure("visible high-rate producer is satisfied", entitlement[1] == 8.f);
    ensure("only visible remainder reaches previews",
           std::fabs(entitlement[2] - 1.5f) < 0.001f &&
           std::fabs(entitlement[3] - 1.5f) < 0.001f);
}

template<> template<>
void prism_preview_policy_object::test<2>()
{
    set_test_name("preview request obeys shared and capture ceilings");
    using LLPrismLens::EOutputRateMode;
    ensure("automatic uses shared preview rate",
           LLPrismPreviewPolicy::requestedHz(
               2.f, EOutputRateMode::AUTOMATIC, 30.f) == 2.f);
    ensure("target rate can lower preview rate",
           LLPrismPreviewPolicy::requestedHz(
               5.f, EOutputRateMode::TARGET_FPS, 1.f) == 1.f);
    ensure("target rate cannot raise shared preview rate",
           LLPrismPreviewPolicy::requestedHz(
               1.f, EOutputRateMode::TARGET_FPS, 30.f) == 1.f);
}

template<> template<>
void prism_preview_policy_object::test<3>()
{
    set_test_name("preview deadline discards backlog rather than catching up");
    const F64 next = LLPrismPreviewPolicy::advanceDeadline(100.0, 2.f);
    ensure("next deadline starts from now", std::fabs(next - 100.5) < 1e-9);
    const F64 after_hitch = LLPrismPreviewPolicy::advanceDeadline(120.0, 2.f);
    ensure("hitch grants one future deadline, not burst credit",
           std::fabs(after_hitch - 120.5) < 1e-9);
}

template<> template<>
void prism_preview_policy_object::test<4>()
{
    set_test_name("stale AO sentinel is replaced by neutral white");
    std::array<F32, 4> reused_ao = { 0.13f, 0.27f, 0.41f, 0.f };
    bool target_bound = false;
    ensure("an incomplete target cannot enter the clear path",
           !LLPrismPreviewPolicy::clearPreviewAO(
               true, false,
               [&]() { target_bound = true; },
               [&](const std::array<F32, 4>& clear_color)
               { reused_ao = clear_color; }));
    ensure("a skipped clear preserves the stale attachment sentinel",
           !target_bound && reused_ao[0] == 0.13f && reused_ao[1] == 0.27f &&
           reused_ao[2] == 0.41f && reused_ao[3] == 0.f);
    const bool clear = LLPrismPreviewPolicy::clearPreviewAO(
        true, true,
        [&]() { target_bound = true; },
        [&](const std::array<F32, 4>& clear_color)
        { reused_ao = clear_color; });
    ensure("preview path requests an attachment clear", clear);
    ensure("preview path binds the target before clearing", target_bound);
    ensure("all stale channels are neutral",
           reused_ao[0] == 1.f && reused_ao[1] == 1.f &&
           reused_ao[2] == 1.f && reused_ao[3] == 1.f);
}

template<> template<>
void prism_preview_policy_object::test<5>()
{
    set_test_name("preview render pack is always the pinned 192 by 108");
    ensure("physical preview width is fixed", LLPrismPreviewPolicy::WIDTH == 192);
    ensure("physical preview height is fixed", LLPrismPreviewPolicy::HEIGHT == 108);
}

template<> template<>
void prism_preview_policy_object::test<6>()
{
    set_test_name("sub-epsilon capacity grants no preview attempt");
    std::array<bool, LLPrismLens::MAX_CAPTURES> preview{};
    std::array<F32, LLPrismLens::MAX_CAPTURES> demand{};
    std::array<F32, LLPrismLens::MAX_CAPTURES> entitlement{};
    preview[0] = true;
    demand[0] = 1.f;
    const F32 remaining = LLPrismPreviewPolicy::waterFillTier(
        preview, demand, LLPrismPreviewPolicy::ENTITLEMENT_EPSILON * 0.5f,
        entitlement);
    ensure("tiny remainder is discarded",
           remaining == 0.f && entitlement[0] == 0.f);
}

template<> template<>
void prism_preview_policy_object::test<7>()
{
    set_test_name("synthetic CPU/GPU reservations charge the independent maximum");
    PrismPreviewBudgetController budget;
    budget.noteFrame(1, 0.0, 16.0, false);
    F64 early_reservation = 0.0;
    ensure("reservation cannot exceed accumulated tokens",
           !budget.reserve(PrismPreviewBudgetController::EOperation::AUXILIARY,
                           early_reservation));
    for (U32 frame = 1; frame <= 20; ++frame)
        budget.noteFrame(frame, frame / 60.0, 16.0, false);
    F64 reservation = 0.0;
    ensure("auxiliary seed reserves within accumulated tokens",
           budget.reserve(PrismPreviewBudgetController::EOperation::AUXILIARY,
                          reservation));
    budget.complete(PrismPreviewBudgetController::EOperation::AUXILIARY,
                    reservation, 0.4, 1.2, true);
    ensure("GPU dominates charged cost",
           std::fabs(budget.stats().mChargedMs - 1.2) < 0.001);
}

template<> template<>
void prism_preview_policy_object::test<8>()
{
    set_test_name("synthetic tail costs throttle, suspend, and recover");
    PrismPreviewBudgetController budget;
    for (U32 frame = 1; frame <= 120; ++frame)
        budget.noteFrame(frame, frame / 60.0, 16.0, false);
    for (U32 violation = 0; violation < 4; ++violation)
        budget.complete(PrismPreviewBudgetController::EOperation::AUXILIARY,
                        0.0, 4.5, 1.0, true);
    ensure("repeated 0.5-fps tail failure suspends", budget.suspended());
    for (U32 clean = 0; clean < 120; ++clean)
        budget.complete(PrismPreviewBudgetController::EOperation::ATLAS,
                        0.0, 0.02, 0.03, true);
    ensure("120 clean samples resume at throttled rate",
           !budget.suspended() && budget.effectiveRate(5.f) <= 0.5f);
}

template<> template<>
void prism_preview_policy_object::test<9>()
{
    set_test_name("synthetic scratch reacquisition is independently counted");
    PrismPreviewBudgetController budget;
    budget.noteScratchReacquisition();
    ensure_equals("scratch miss is exposed", budget.stats().mScratchReacquisitions, 1u);
}
} // namespace tut
