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
    reused_ao = LLPrismPreviewPolicy::neutralAO();
    ensure("all stale channels are neutral",
           reused_ao[0] == 1.f && reused_ao[1] == 1.f &&
           reused_ao[2] == 1.f && reused_ao[3] == 1.f);
}
} // namespace tut
