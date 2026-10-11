/**
 * @file llprismpreview.cpp
 * @brief Demand and scoped render policy for Director VCam previews.
 */

#include "llviewerprecompiledheaders.h"

#include "llprismpreview.h"

#include "llviewershadermgr.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "pipeline.h"
#include "llenvironment.h"
#include "llsettingssky.h"
#include "llappviewer.h"

#include <algorithm>

namespace
{
constexpr LLPrismPreviewService::VisibilityOwner PAGE_VISIBILITY_OWNER = 1;
}

void PrismPreviewBudgetController::reset()
{
    *this = PrismPreviewBudgetController();
}

F64 PrismPreviewBudgetController::percentile95(
    const std::array<F64, SAMPLE_COUNT>& values, U32 count) const
{
    if (count == 0) return 0.0;
    std::array<F64, SAMPLE_COUNT> ordered = values;
    const U32 bounded = llmin(count, SAMPLE_COUNT);
    std::sort(ordered.begin(), ordered.begin() + bounded);
    return ordered[llmin(bounded - 1, static_cast<U32>(ceil(0.95 * bounded)) - 1)];
}

void PrismPreviewBudgetController::noteFrame(
    U32 frame, F64 now, F64 frame_ms, bool preview_work)
{
    mCurrentFrame = frame;
    mNow = now;
    const U32 cell = frame % FRAME_COUNT;
    if (mFrameIds[cell] != frame)
    {
        mFrameIds[cell] = frame;
        mFrameCharges[cell] = 0.0;
    }
    mFrameTimes[cell] = now;
    if (preview_work && mSkipColdPreviewFrame)
    {
        mSkipColdPreviewFrame = false;
        return;
    }
    if (mColdCompletions < COLD_OPERATIONS) return;
    ++mStats.mPresentedFrames;
    ++mFramesSinceSample;
    if (preview_work)
    {
        mPreviewFrames[mPreviewCursor++ % SAMPLE_COUNT] = frame_ms;
        mPreviewCount = llmin(mPreviewCount + 1, SAMPLE_COUNT);
    }
    else
    {
        mBaselineFrames[mBaselineCursor++ % SAMPLE_COUNT] = frame_ms;
        mBaselineCount = llmin(mBaselineCount + 1, SAMPLE_COUNT);
    }
    if (mBaselineCount > 0 && mPreviewCount > 0)
    {
        mStats.mFrameDeltaP95Ms =
            percentile95(mPreviewFrames, mPreviewCount) -
            percentile95(mBaselineFrames, mBaselineCount);
    }
}

bool PrismPreviewBudgetController::reserve(EOperation operation,
                                            F64& reservation_ms)
{
    if (mRateStep == 3 &&
        mNow - mLastFloorAdmissionAt < FLOOR_INTERVAL_SECONDS)
    {
        ++mStats.mDeferrals;
        return false;
    }
    const F64 seed = operation == EOperation::AUXILIARY ? 1.5 : 0.05;
    F64 recent_max = 0.0;
    for (U32 i = 0; i < mSampleCount; ++i)
        recent_max = llmax(recent_max, mChargedSamples[i]);
    const U32 operation_index = operation == EOperation::AUXILIARY ? 0u : 1u;
    reservation_ms = llmax(mPredictionMs[operation_index], llmax(seed, llmax(mStats.mCpuP95Ms,
        llmax(mStats.mGpuP95Ms, recent_max))));
    F64 allowance = 0.0;
    F64 charged = 0.0;
    for (U32 i = 0; i < FRAME_COUNT; ++i)
    {
        if (mFrameIds[i] != 0 && mNow - mFrameTimes[i] <= 2.0)
        {
            allowance += LLPrismPreviewPolicy::BUDGET_MS_PER_FRAME;
            charged += mFrameCharges[i];
        }
    }
    if (charged + mReservedMs + reservation_ms > allowance)
    {
        if (mNow - mLastFloorAdmissionAt < FLOOR_INTERVAL_SECONDS)
        {
            ++mStats.mDeferrals;
            return false;
        }
        // Keep one measurement arriving at the 0.5-fps floor even when the
        // current prediction is larger than the accumulated allowance.
        mLastFloorAdmissionAt = mNow;
    }
    if (mRateStep == 3) mLastFloorAdmissionAt = mNow;
    mReservedMs += reservation_ms;
    if (mColdAdmissions++ >= COLD_OPERATIONS)
        ++mStats.mAdmittedOperations;
    return true;
}

void PrismPreviewBudgetController::clearDecisionWindow()
{
    mCpuSamples.fill(0.0);
    mGpuSamples.fill(0.0);
    mChargedSamples.fill(0.0);
    mSampleFrameSpans.fill(0);
    for (auto& samples : mCpuOperationSamples) samples.fill(0.0);
    for (auto& samples : mGpuOperationSamples) samples.fill(0.0);
    mOperationSampleCursor.fill(0);
    mOperationSampleCount.fill(0);
    mBaselineFrames.fill(0.0);
    mPreviewFrames.fill(0.0);
    mSampleCursor = 0;
    mSampleCount = 0;
    mBaselineCursor = 0;
    mBaselineCount = 0;
    mPreviewCursor = 0;
    mPreviewCount = 0;
    mFramesSinceSample = 0;
    mCleanSamples = 0;
    mStats.mCpuP95Ms = 0.0;
    mStats.mGpuP95Ms = 0.0;
    mStats.mCpuP95MsByOperation.fill(0.0);
    mStats.mGpuP95MsByOperation.fill(0.0);
    mStats.mFrameDeltaP95Ms = 0.0;
}

void PrismPreviewBudgetController::stepDown()
{
    if (mRateStep < 3) ++mRateStep;
    clearDecisionWindow();
}

bool PrismPreviewBudgetController::canAttempt() const
{
    return true;
}

void PrismPreviewBudgetController::complete(
    EOperation operation, F64 reservation_ms, F64 cpu_ms, F64 gpu_ms, bool gpu_valid)
{
    mReservedMs = llmax(0.0, mReservedMs - reservation_ms);
    const F64 charged = llmax(cpu_ms, gpu_valid ? gpu_ms : 0.0);
    const U32 operation_index = operation == EOperation::AUXILIARY ? 0u : 1u;
    if (mColdCompletions++ < COLD_OPERATIONS)
    {
        // Scratch/FBO allocation dominates the first few operations after a
        // reset. Exclude those operations and their following presented frame
        // from policy prediction, decisions, and reported statistics.
        mSkipColdPreviewFrame = true;
        return;
    }
    const U32 sample = mSampleCursor++ % SAMPLE_COUNT;
    mCpuSamples[sample] = cpu_ms;
    mGpuSamples[sample] = gpu_valid ? gpu_ms : 0.0;
    mChargedSamples[sample] = charged;
    mSampleFrameSpans[sample] = llmax(1u, mFramesSinceSample);
    mFramesSinceSample = 0;
    mSampleCount = llmin(mSampleCount + 1, SAMPLE_COUNT);
    mStats.mCpuMs += cpu_ms;
    if (gpu_valid)
    {
        mStats.mGpuMs += gpu_ms;
        ++mStats.mGpuSamples;
    }
    mStats.mChargedMs += charged;
    ++mStats.mOperations;
    ++mStats.mCpuSamples;
    mStats.mCpuMsByOperation[operation_index] += cpu_ms;
    ++mStats.mOperationsByType[operation_index];
    if (gpu_valid)
    {
        mStats.mGpuMsByOperation[operation_index] += gpu_ms;
        ++mStats.mGpuSamplesByType[operation_index];
    }
    const U32 operation_sample =
        mOperationSampleCursor[operation_index]++ % SAMPLE_COUNT;
    mCpuOperationSamples[operation_index][operation_sample] = cpu_ms;
    mGpuOperationSamples[operation_index][operation_sample] =
        gpu_valid ? gpu_ms : 0.0;
    mOperationSampleCount[operation_index] = llmin(
        mOperationSampleCount[operation_index] + 1, SAMPLE_COUNT);
    mStats.mCpuP95MsByOperation[operation_index] = percentile95(
        mCpuOperationSamples[operation_index],
        mOperationSampleCount[operation_index]);
    mStats.mGpuP95MsByOperation[operation_index] = percentile95(
        mGpuOperationSamples[operation_index],
        mOperationSampleCount[operation_index]);
    mStats.mCpuP95Ms = percentile95(mCpuSamples, mSampleCount);
    mStats.mGpuP95Ms = percentile95(mGpuSamples, mSampleCount);
    mPredictionMs[operation_index] = llmax(
        operation == EOperation::AUXILIARY ? 1.5 : 0.05,
        mStats.mCpuP95MsByOperation[operation_index]);
    if (gpu_valid)
        mPredictionMs[operation_index] = llmax(
            mPredictionMs[operation_index],
            mStats.mGpuP95MsByOperation[operation_index]);
    mStats.mMaxOperationMs = llmax(mStats.mMaxOperationMs, charged);
    const U32 frame_cell = mCurrentFrame % FRAME_COUNT;
    if (mFrameIds[frame_cell] == mCurrentFrame)
        mFrameCharges[frame_cell] += charged;
    F64 rolling_charge = 0.0;
    F64 max_operation = 0.0;
    U32 rolling_frames = 0;
    for (U32 i = 0; i < mSampleCount; ++i)
    {
        rolling_charge += mChargedSamples[i];
        max_operation = llmax(max_operation, mChargedSamples[i]);
        rolling_frames += mSampleFrameSpans[i];
    }
    const bool violation = mSampleCount >= DECISION_SAMPLES &&
        ((rolling_frames > 0 &&
          rolling_charge / static_cast<F64>(rolling_frames) >
              LLPrismPreviewPolicy::BUDGET_MS_PER_FRAME) ||
         mStats.mFrameDeltaP95Ms > 2.0 ||
         max_operation > LLPrismPreviewPolicy::MAX_OPERATION_MS);
    if (violation)
        stepDown();
    else if (++mCleanSamples >= DECISION_SAMPLES && mRateStep > 0)
    {
        --mRateStep;
        clearDecisionWindow();
    }
    else
    {
        mCleanSamples = llmin(mCleanSamples, DECISION_SAMPLES);
    }
}

F32 PrismPreviewBudgetController::effectiveRate(F32 requested) const
{
    static constexpr F32 steps[] = { 5.f, 2.f, 1.f, 0.5f };
    const F32 ceiling = steps[llmin(mRateStep, 3u)];
    const F32 warming = mColdCompletions < COLD_OPERATIONS ? 0.5f : ceiling;
    return llmin(requested, warming);
}

LLPrismPreviewOperation::LLPrismPreviewOperation(
    PrismPreviewBudgetController::EOperation operation,
    const LLPrismLens::CaptureHandle& capture)
    : mOperation(operation), mCapture(capture)
{
    mAdmitted = LLPrismPreviewService::instance().beginOperation(
        operation, capture, mReservationMs, mQuerySlot);
    if (mAdmitted) mTimer.reset();
}

LLPrismPreviewOperation::~LLPrismPreviewOperation()
{
    if (mAdmitted)
        LLPrismPreviewService::instance().endOperation(
            mOperation, mCapture, mReservationMs, mQuerySlot,
            static_cast<F64>(mTimer.getElapsedTimeF64()) * 1000.0);
}

bool LLPrismPreviewService::beginOperation(
    PrismPreviewBudgetController::EOperation operation,
    const LLPrismLens::CaptureHandle& capture, F64& reservation_ms, S32& query_slot)
{
    pollQueries();
    const U32 capture_slot = LLPrismLens::captureSlot(capture);
    if (mSelfTestPhase == ESelfTestPhase::ENABLED &&
        operation == PrismPreviewBudgetController::EOperation::AUXILIARY &&
        capture_slot < mSelfTestRequested.size())
    {
        ++mSelfTestRequested[capture_slot];
    }
    if (!mBudget.reserve(operation, reservation_ms))
    {
        markChanged();
        return false;
    }
    mLastPreviewWorkFrame = gFrameCount;
    if (mSelfTestPhase == ESelfTestPhase::ENABLED &&
        operation == PrismPreviewBudgetController::EOperation::AUXILIARY &&
        capture_slot < mSelfTestAdmitted.size())
    {
        ++mSelfTestAdmitted[capture_slot];
    }
    query_slot = -1;
    if (glQueryCounter)
    {
        for (U32 i = 0; i < mQueries.size(); ++i)
        {
            QueryEntry& query = mQueries[i];
            if (query.mPending) continue;
            if (query.mStart == 0)
            {
                glGenQueries(1, &query.mStart);
                glGenQueries(1, &query.mEnd);
            }
            glQueryCounter(query.mStart, GL_TIMESTAMP);
            query.mBudgetGeneration = mBudgetGeneration;
            query_slot = static_cast<S32>(i);
            break;
        }
    }
    return true;
}

void LLPrismPreviewService::endOperation(
    PrismPreviewBudgetController::EOperation operation,
    const LLPrismLens::CaptureHandle&, F64 reservation_ms,
    S32 query_slot, F64 cpu_ms)
{
    if (query_slot >= 0)
    {
        QueryEntry& query = mQueries[query_slot];
        glQueryCounter(query.mEnd, GL_TIMESTAMP);
        query.mIssuedFrame = gFrameCount;
        query.mReservationMs = reservation_ms;
        query.mCpuMs = cpu_ms;
        query.mOperation = operation;
        query.mPending = true;
    }
    else
    {
        mBudget.complete(operation, reservation_ms, cpu_ms, 0.0, false);
        if (mSelfTestPhase == ESelfTestPhase::ENABLED)
        {
            recordSelfTestOperation(operation, cpu_ms, 0.0, false);
            mSelfTestMaxOperationMs = llmax(mSelfTestMaxOperationMs, cpu_ms);
        }
    }
}

void LLPrismPreviewService::recordSelfTestOperation(
    PrismPreviewBudgetController::EOperation operation,
    F64 cpu_ms, F64 gpu_ms, bool gpu_valid)
{
    const U32 operation_index =
        operation == PrismPreviewBudgetController::EOperation::AUXILIARY ? 0u : 1u;
    U32& cpu_count = mSelfTestCpuSampleCount[operation_index];
    mSelfTestCpuSamples[operation_index][
        mSelfTestCpuSampleCursor[operation_index]++ % 128] = cpu_ms;
    cpu_count = llmin(cpu_count + 1, 128u);
    if (gpu_valid)
    {
        U32& gpu_count = mSelfTestGpuSampleCount[operation_index];
        mSelfTestGpuSamples[operation_index][
            mSelfTestGpuSampleCursor[operation_index]++ % 128] = gpu_ms;
        gpu_count = llmin(gpu_count + 1, 128u);
    }
}

void LLPrismPreviewService::pollQueries()
{
    for (QueryEntry& query : mQueries)
    {
        if (!query.mPending || gFrameCount - query.mIssuedFrame < 3) continue;
        GLuint available = GL_FALSE;
        glGetQueryObjectuiv(query.mEnd, GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == GL_FALSE) continue;
        GLuint64 start = 0;
        GLuint64 end = 0;
        glGetQueryObjectui64v(query.mStart, GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(query.mEnd, GL_QUERY_RESULT, &end);
        const F64 gpu_ms = end >= start
            ? static_cast<F64>(end - start) / 1000000.0 : 0.0;
        if (query.mBudgetGeneration == mBudgetGeneration)
        {
            mBudget.complete(query.mOperation, query.mReservationMs,
                             query.mCpuMs, gpu_ms, end >= start);
        }
        if (query.mBudgetGeneration == mBudgetGeneration &&
            mSelfTestPhase == ESelfTestPhase::ENABLED)
        {
            recordSelfTestOperation(query.mOperation, query.mCpuMs,
                                    gpu_ms, end >= start);
            mSelfTestMaxOperationMs = llmax(
                mSelfTestMaxOperationMs, llmax(query.mCpuMs, gpu_ms));
        }
        query.mPending = false;
    }
}

void LLPrismPreviewService::runSelfTest(F32 seconds)
{
    mSelfTestSeconds = llmax(20.f, seconds);
    mSelfTestPhase = ESelfTestPhase::BASELINE;
    mSelfTestPhaseStarted = LLTimer::getTotalSeconds();
    mSelfTestStartStats = mBudget.stats();
    mSelfTestConfigurationRevision = LLPrismLens::configurationRevision();
    mSelfTestVisibleEntitlement.fill(0.f);
    mSelfTestRequested.fill(0);
    mSelfTestAdmitted.fill(0);
    mSelfTestProduced.fill(0);
    mSelfTestBaselineFrameCount = 0;
    mSelfTestEnabledFrameCount = 0;
    mSelfTestBaselineFrameCursor = 0;
    mSelfTestEnabledFrameCursor = 0;
    LL_INFOS("VCamPreviewSelfTest") << "[VCamPreviewSelfTest] WARMING phase=baseline" << LL_ENDL;
}

void LLPrismPreviewService::tickSelfTest(bool prism_enabled,
                                         bool deferred_rendering,
                                         bool has_captures)
{
    static LLCachedControl<bool> requested(
        gSavedSettings, "PrismPreviewSelfTest", false);
    if (requested)
    {
        gSavedSettings.setBOOL("PrismPreviewSelfTest", false);
        runSelfTest();
    }
    if (mSelfTestPhase == ESelfTestPhase::IDLE) return;
    U32 demand_count = 0;
    for (const DemandEntry& entry : mDemand) demand_count += entry.mDemanded ? 1u : 0u;
    static LLCachedControl<bool> previews_enabled(
        gSavedSettings, "PrismMonitorPreviewsEnabled", true);
    static LLCachedControl<F32> preview_fps(
        gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
    const char* missing = nullptr;
    if (!prism_enabled) missing = "prism_disabled";
    else if (!deferred_rendering) missing = "deferred_rendering_disabled";
    else if (!has_captures) missing = "no_captures";
    else if (!previews_enabled) missing = "previews_disabled";
    else if (!anyVisibleConsumer()) missing = "page_hidden";
    else if (demand_count != 8) missing = "eight_valid_captures_required";
    else if (fabsf(static_cast<F32>(preview_fps) - 1.f) > 0.001f)
        missing = "preview_fps_not_one";
    else if (LLPrismLens::configurationRevision() != mSelfTestConfigurationRevision)
        missing = "configuration_changed";
    if (missing)
    {
        LL_INFOS("VCamPreviewSelfTest")
            << "[VCamPreviewSelfTest] INCONCLUSIVE cpu_ms=0 gpu_ms=0 charged_ms=0 "
               "scratch_reacquire=0 p95_frame_delta_ms=0 max_op_ms=0 "
               "cpu_coverage=0 gpu_coverage=0 reason=" << missing
            << LL_ENDL;
        mSelfTestPhase = ESelfTestPhase::IDLE;
        return;
    }
    const F64 now = LLTimer::getTotalSeconds();
    if (mSelfTestPhase == ESelfTestPhase::BASELINE &&
        now - mSelfTestPhaseStarted >= 5.0)
    {
        mSelfTestPhase = ESelfTestPhase::WARMING;
        mSelfTestPhaseStarted = now;
        const LLPrismLens::RegistrySnapshot snapshot =
            LLPrismLens::registrySnapshot();
        for (const LLPrismLens::CaptureDefinition& capture : snapshot.mCaptures)
        {
            if (capture.mSlot < mSelfTestVisibleEntitlement.size() &&
                !capture.mRuntime.mPreviewOnly)
            {
                mSelfTestVisibleEntitlement[capture.mSlot] =
                    capture.mRuntime.mCadenceEntitlementHz;
            }
        }
    }
    if (mSelfTestPhase == ESelfTestPhase::WARMING &&
        !mBudget.warming())
    {
        mSelfTestPhase = ESelfTestPhase::ENABLED;
        mSelfTestPhaseStarted = now;
        mSelfTestStartStats = mBudget.stats();
        mSelfTestRequested.fill(0);
        mSelfTestAdmitted.fill(0);
        mSelfTestProduced.fill(0);
        mSelfTestCpuSampleCount.fill(0);
        mSelfTestGpuSampleCount.fill(0);
        mSelfTestCpuSampleCursor.fill(0);
        mSelfTestGpuSampleCursor.fill(0);
        mSelfTestMaxOperationMs = 0.0;
    }
    else if (mSelfTestPhase == ESelfTestPhase::WARMING &&
             now - mSelfTestPhaseStarted >= 30.0)
    {
        LL_INFOS("VCamPreviewSelfTest")
            << "[VCamPreviewSelfTest] INCONCLUSIVE cpu_coverage=0 gpu_coverage=0 reason="
            << "warmup_timeout_no_samples"
            << LL_ENDL;
        mSelfTestPhase = ESelfTestPhase::IDLE;
        return;
    }
    if (mSelfTestPhase != ESelfTestPhase::ENABLED ||
        now - mSelfTestPhaseStarted < mSelfTestSeconds) return;
    const auto& stats = mBudget.stats();
    const auto delta_u32 = [](U32 current, U32 start)
    {
        return current >= start ? current - start : 0u;
    };
    const auto delta_f64 = [](F64 current, F64 start)
    {
        return current >= start ? current - start : 0.0;
    };
    const U32 operations = delta_u32(
        stats.mOperations, mSelfTestStartStats.mOperations);
    const U32 admitted = delta_u32(
        stats.mAdmittedOperations, mSelfTestStartStats.mAdmittedOperations);
    const U32 cpu_samples = delta_u32(
        stats.mCpuSamples, mSelfTestStartStats.mCpuSamples);
    const U32 gpu_samples = delta_u32(
        stats.mGpuSamples, mSelfTestStartStats.mGpuSamples);
    const U32 frames = delta_u32(
        stats.mPresentedFrames, mSelfTestStartStats.mPresentedFrames);
    const F64 cpu = delta_f64(stats.mCpuMs, mSelfTestStartStats.mCpuMs);
    const F64 gpu = delta_f64(stats.mGpuMs, mSelfTestStartStats.mGpuMs);
    const F64 charged = delta_f64(
        stats.mChargedMs, mSelfTestStartStats.mChargedMs);
    const F64 avg = frames ? charged / frames : 0.0;
    const F64 cpu_avg = frames ? cpu / frames : 0.0;
    const F64 gpu_avg = frames ? gpu / frames : 0.0;
    const auto self_test_frame_p95 = [](const std::array<F64, 512>& samples,
                                        U32 count)
    {
        if (count == 0) return 0.0;
        std::array<F64, 512> ordered = samples;
        std::sort(ordered.begin(), ordered.begin() + count);
        return ordered[llmin(count - 1,
            static_cast<U32>(ceil(0.95 * count)) - 1)];
    };
    const F64 frame_delta_p95 = self_test_frame_p95(
        mSelfTestEnabledFrames, mSelfTestEnabledFrameCount) -
        self_test_frame_p95(mSelfTestBaselineFrames, mSelfTestBaselineFrameCount);
    const bool frame_covered = mSelfTestBaselineFrameCount >= 8 &&
        mSelfTestEnabledFrameCount >= 8;
    const F64 cpu_coverage = admitted
        ? static_cast<F64>(cpu_samples) / admitted : 0.0;
    const F64 gpu_coverage = admitted
        ? static_cast<F64>(gpu_samples) / admitted : 0.0;
    const bool covered = admitted > 0 && cpu_coverage >= 0.95 &&
        gpu_coverage >= 0.95;
    bool serviced = true;
    for (U32 count : mSelfTestProduced) serviced = serviced && count > 0;
    bool visible_entitlement_preserved = true;
    F64 visible_entitlement_before = 0.0;
    F64 visible_entitlement_after = 0.0;
    for (F32 entitlement : mSelfTestVisibleEntitlement)
        visible_entitlement_before += entitlement;
    const LLPrismLens::RegistrySnapshot final_snapshot =
        LLPrismLens::registrySnapshot();
    for (const LLPrismLens::CaptureDefinition& capture : final_snapshot.mCaptures)
    {
        if (capture.mSlot < mSelfTestVisibleEntitlement.size() &&
            mSelfTestVisibleEntitlement[capture.mSlot] > 0.f)
        {
            visible_entitlement_after += capture.mRuntime.mCadenceEntitlementHz;
            if (capture.mRuntime.mCadenceEntitlementHz + 0.001f <
                mSelfTestVisibleEntitlement[capture.mSlot])
            {
                visible_entitlement_preserved = false;
            }
        }
    }
    const bool bounds_pass = cpu_avg <= 0.300 && gpu_avg <= 0.300 &&
        avg <= 0.300 && frame_delta_p95 <= 2.0 &&
        mSelfTestMaxOperationMs <= 4.0 &&
        delta_u32(stats.mScratchReacquisitions,
                  mSelfTestStartStats.mScratchReacquisitions) == 0 &&
        visible_entitlement_preserved;
    const bool pass = covered && frame_covered && serviced && bounds_pass;
    const char* verdict = !bounds_pass || (covered && frame_covered && !serviced)
        ? "FAIL" : (pass ? "PASS" : "INCONCLUSIVE");
    const char* reason = pass || !bounds_pass ||
        (covered && frame_covered && !serviced)
        ? "none"
        : (operations == 0 ? "no_preview_operations"
           : (!frame_covered ? "presented_frame_coverage"
           : (!glQueryCounter ? "gpu_timestamps_unavailable"
              : "gpu_timestamp_coverage")));
    const auto self_test_p95 = [](const std::array<F64, 128>& samples,
                                  U32 count)
    {
        if (count == 0) return 0.0;
        std::array<F64, 128> ordered = samples;
        std::sort(ordered.begin(), ordered.begin() + count);
        return ordered[llmin(count - 1,
            static_cast<U32>(ceil(0.95 * count)) - 1)];
    };
    const F64 cpu_aux_p95 = self_test_p95(
        mSelfTestCpuSamples[0], mSelfTestCpuSampleCount[0]);
    const F64 gpu_aux_p95 = self_test_p95(
        mSelfTestGpuSamples[0], mSelfTestGpuSampleCount[0]);
    const F64 cpu_atlas_p95 = self_test_p95(
        mSelfTestCpuSamples[1], mSelfTestCpuSampleCount[1]);
    const F64 gpu_atlas_p95 = self_test_p95(
        mSelfTestGpuSamples[1], mSelfTestGpuSampleCount[1]);
    LL_INFOS("VCamPreviewSelfTest") << "[VCamPreviewSelfTest] "
        << verdict
        << " cpu_ms=" << cpu << " gpu_ms=" << gpu
        << " charged_ms=" << charged << " avg_charged_ms_per_frame=" << avg
        << " avg_cpu_ms_per_frame=" << cpu_avg
        << " avg_gpu_ms_per_frame=" << gpu_avg
        << " scratch_reacquire="
        << delta_u32(stats.mScratchReacquisitions,
                     mSelfTestStartStats.mScratchReacquisitions)
        << " p95_frame_delta_ms=" << frame_delta_p95
        << " cpu_p95_ms=" << llmax(cpu_aux_p95, cpu_atlas_p95)
        << " gpu_p95_ms=" << llmax(gpu_aux_p95, gpu_atlas_p95)
        << " cpu_aux_p95_ms=" << cpu_aux_p95
        << " gpu_aux_p95_ms=" << gpu_aux_p95
        << " cpu_atlas_p95_ms=" << cpu_atlas_p95
        << " gpu_atlas_p95_ms=" << gpu_atlas_p95
        << " cpu_aux_ms="
        << delta_f64(stats.mCpuMsByOperation[0],
                     mSelfTestStartStats.mCpuMsByOperation[0])
        << " gpu_aux_ms="
        << delta_f64(stats.mGpuMsByOperation[0],
                     mSelfTestStartStats.mGpuMsByOperation[0])
        << " cpu_atlas_ms="
        << delta_f64(stats.mCpuMsByOperation[1],
                     mSelfTestStartStats.mCpuMsByOperation[1])
        << " gpu_atlas_ms="
        << delta_f64(stats.mGpuMsByOperation[1],
                     mSelfTestStartStats.mGpuMsByOperation[1])
        << " atlas_updates="
        << delta_u32(stats.mOperationsByType[1],
                     mSelfTestStartStats.mOperationsByType[1])
        << " max_op_ms=" << mSelfTestMaxOperationMs << " frames=" << frames
        << " cpu_coverage=" << cpu_coverage
        << " gpu_coverage=" << gpu_coverage
        << " frame_coverage=" << (frame_covered ? 1 : 0)
        << " timing_unavailable_operations="
        << (admitted >= gpu_samples ? admitted - gpu_samples : 0u)
        << " coverage=" << (covered ? 1 : 0)
        << " visible_entitlement_preserved=" << (visible_entitlement_preserved ? 1 : 0)
        << " visible_entitlement_before_hz=" << visible_entitlement_before
        << " visible_entitlement_after_hz=" << visible_entitlement_after
        << " reason=" << reason
        << " deferred=" << delta_u32(
            stats.mDeferrals, mSelfTestStartStats.mDeferrals)
        << " duration_s=" << (now - mSelfTestPhaseStarted);
    for (U32 slot = 0; slot < mSelfTestProduced.size(); ++slot)
    {
        LL_CONT << " c" << slot << "_requested=" << mSelfTestRequested[slot]
                << " c" << slot << "_admitted=" << mSelfTestAdmitted[slot]
                << " c" << slot << "_produced=" << mSelfTestProduced[slot]
                << " c" << slot << "_achieved_fps="
                << (now > mSelfTestPhaseStarted
                    ? mSelfTestProduced[slot] / (now - mSelfTestPhaseStarted) : 0.0);
    }
    LL_CONT << LL_ENDL;
    mSelfTestPhase = ESelfTestPhase::IDLE;
}

namespace LLPrismPreview
{
boost::signals2::connection connectChanged(
    const LLPrismPreviewService::changed_signal_t::slot_type& slot)
{
    return LLPrismPreviewService::instance().connectChanged(slot);
}

void runSelfTest(F32 seconds)
{
    LLPrismPreviewService::instance().runSelfTest(seconds);
}
}

LLPrismPreviewService& LLPrismPreviewService::instance()
{
    static LLPrismPreviewService service;
    return service;
}

boost::signals2::connection LLPrismPreviewService::connectChanged(
    const changed_signal_t::slot_type& slot)
{
    return mChanged.connect(slot);
}

void LLPrismPreviewService::markChanged()
{
    ++mRevision;
    mChanged(mRevision);
}

void LLPrismPreviewService::setDemand(const LLPrismLens::CaptureHandle& capture,
                                      bool demanded)
{
    if (capture.mId.isNull()) return;
    const U32 slot = LLPrismLens::captureSlot(capture);
    if (slot >= mDemand.size())
    {
        if (!demanded)
        {
            for (DemandEntry& stale : mDemand)
                if (stale.mCapture.mId == capture.mId) stale = DemandEntry{};
        }
        return;
    }
    DemandEntry& entry = mDemand[slot];
    const bool same_publication = entry.mCapture.mId == capture.mId &&
        entry.mCapture.mGeneration == capture.mGeneration;
    const bool changed = !same_publication || entry.mDemanded != demanded;
    if (!same_publication)
    {
        mNextAtlasDue[slot] = 0.0;
        mSelection[slot] = LLPrismLens::DisplayHandle{};
        mTiles[slot] = TilePublication{};
    }
    entry = demanded
        ? DemandEntry{ capture, true,
                       same_publication ? entry.mPublicationSerial : 0,
                       same_publication ? entry.mAtlasSerial : 0,
                       same_publication ? entry.mPreviewQuality : false }
        : DemandEntry{};
    if (changed) markChanged();
}

void LLPrismPreviewService::clearDemand()
{
    for (DemandEntry& entry : mDemand) entry = DemandEntry{};
    for (LLPrismLens::DisplayHandle& selection : mSelection)
        selection = LLPrismLens::DisplayHandle{};
    for (TilePublication& tile : mTiles) tile = TilePublication{};
    mNextAtlasDue.fill(0.0);
    markChanged();
}

void LLPrismPreviewService::setPageVisible(bool visible)
{
    setConsumerVisible(PAGE_VISIBILITY_OWNER, visible);
}

void LLPrismPreviewService::setConsumerVisible(VisibilityOwner owner, bool visible)
{
    if (owner == 0) return;
    VisibilityEntry* empty = nullptr;
    for (VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner == owner)
        {
            if (entry.mVisible == visible) return;
            entry.mVisible = visible;
            markChanged();
            return;
        }
        if (!empty && entry.mOwner == 0) empty = &entry;
    }
    if (empty)
    {
        *empty = VisibilityEntry{ owner, visible };
        markChanged();
        return;
    }
    LL_WARNS("PrismPreview")
        << "Visibility owner table is full; ignoring owner " << owner
        << LL_ENDL;
}

void LLPrismPreviewService::releaseConsumer(VisibilityOwner owner)
{
    for (VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner == owner)
        {
            entry = VisibilityEntry{};
            markChanged();
            return;
        }
    }
}

void LLPrismPreviewService::setMonitorSelection(
    U32 capture_slot, const LLPrismLens::DisplayHandle& display)
{
    if (capture_slot >= mSelection.size()) return;
    mSelection[capture_slot] = display;
    DemandEntry& entry = mDemand[capture_slot];
    if (entry.mPublicationSerial > 0)
        entry.mAtlasSerial = entry.mPublicationSerial - 1;
    mNextAtlasDue[capture_slot] = 0.0;
    markChanged();
}

bool LLPrismPreviewService::bindAtlas(const AtlasHandle& handle,
                                      S32 texture_channel)
{
    if (handle.mGeneration == 0 || handle.mGeneration != mAtlasGeneration ||
        !mAtlas.isComplete() || texture_channel < 0)
    {
        return false;
    }
    mAtlas.bindTexture(0, texture_channel, LLTexUnit::TFO_BILINEAR);
    return true;
}

LLPrismPreviewService::TilePublication LLPrismPreviewService::tileState(
    const LLPrismLens::CaptureHandle& capture) const
{
    for (U32 slot = 0; slot < mDemand.size(); ++slot)
    {
        const DemandEntry& entry = mDemand[slot];
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration)
        {
            TilePublication tile = mTiles[slot];
            static LLCachedControl<bool> previews_enabled(
                gSavedSettings, "PrismMonitorPreviewsEnabled", true);
            LLPrismLens::EActivityState activity = LLPrismLens::EActivityState::IDLE;
            bool healthy = false;
            LLPrismLens::previewTileActivity(entry.mCapture, activity, healthy);
            if (tile.mState == ETileState::FAILED ||
                tile.mState == ETileState::SHADER_UNAVAILABLE)
            {
                return tile;
            }
            if (!healthy) tile.mState = ETileState::OFFLINE;
            else if (!previews_enabled || activity == LLPrismLens::EActivityState::PAUSED)
                tile.mState = ETileState::PAUSED;
            else if (mBudget.warming()) tile.mState = ETileState::WARMING;
            else if (mBudget.throttled() ||
                      activity == LLPrismLens::EActivityState::THROTTLED)
                tile.mState = ETileState::THROTTLED;
            else if (entry.mPublicationSerial > tile.mPublicationSerial)
                tile.mState = ETileState::HELD;
            return tile;
        }
    }
    return TilePublication{};
}

bool LLPrismPreviewService::anyVisibleConsumer() const
{
    for (const VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner != 0 && entry.mVisible) return true;
    }
    return false;
}

bool LLPrismPreviewService::anyDemand() const
{
    for (const DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded) return true;
    }
    return false;
}

bool LLPrismPreviewService::prepareFrame()
{
    static LLCachedControl<bool> previews_enabled(
        gSavedSettings, "PrismMonitorPreviewsEnabled", true);
    static LLCachedControl<F32> preview_fps(
        gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
    const bool enabled = previews_enabled;
    const F32 configured_fps = preview_fps;
    const F64 frame_ms = static_cast<F64>(gFrameIntervalSeconds) * 1000.0;
    if (mSelfTestPhase == ESelfTestPhase::BASELINE)
    {
        mSelfTestBaselineFrames[mSelfTestBaselineFrameCursor++ % 512] = frame_ms;
        mSelfTestBaselineFrameCount = llmin(mSelfTestBaselineFrameCount + 1, 512u);
    }
    else if (mSelfTestPhase == ESelfTestPhase::ENABLED)
    {
        mSelfTestEnabledFrames[mSelfTestEnabledFrameCursor++ % 512] = frame_ms;
        mSelfTestEnabledFrameCount = llmin(mSelfTestEnabledFrameCount + 1, 512u);
    }
    if (enabled != mLastPreviewsEnabled ||
        fabsf(configured_fps - mLastPreviewFps) > 0.001f)
    {
        mBudget.reset();
        ++mBudgetGeneration;
        mLastPreviewFps = configured_fps;
    }
    if (enabled)
    {
        pollQueries();
        mBudget.noteFrame(gFrameCount, LLTimer::getTotalSeconds(),
            frame_ms,
            mLastPreviewWorkFrame != std::numeric_limits<U32>::max() &&
                mLastPreviewWorkFrame + 1 == gFrameCount);
    }
    if (enabled != mLastPreviewsEnabled)
    {
        mLastPreviewsEnabled = enabled;
        const F64 now = LLTimer::getTotalSeconds();
        const F64 next = now + 1.0 /
            llclamp(static_cast<F64>(configured_fps), 0.5, 5.0);
        mNextAtlasDue.fill(next);
        if (!enabled)
        {
            for (DemandEntry& entry : mDemand)
                entry.mAtlasSerial = entry.mPublicationSerial;
        }
        markChanged();
    }
    if (!enabled || !anyVisibleConsumer() || !anyDemand() ||
        mSelfTestPhase == ESelfTestPhase::BASELINE || !mBudget.canAttempt())
    {
        return false;
    }
    // Deliberately no GL work here: this runs before the complete auxiliary
    // render-state scope. Cold allocation is performed by ensureScratchReady().
    return true;
}

bool LLPrismPreviewService::ensureScratchReady()
{
    const F64 now = LLTimer::getTotalSeconds();
    if (now < mScratchRetryAfter) return false;
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("VCam preview scratch warmup");
    if (gPipeline.bindPrismPreviewScratch())
    {
        mScratchWarmed = true;
        return true;
    }
    if (mScratchWarmed) mBudget.noteScratchReacquisition();
    if (!gPipeline.allocatePrismPreviewScratch())
    {
        mScratchRetryAfter = now + 1.0;
        return false;
    }
    mScratchRetryAfter = 0.0;
    const bool ready = gPipeline.bindPrismPreviewScratch();
    mScratchWarmed = ready;
    return ready;
}

bool LLPrismPreviewService::isDemanded(
    const LLPrismLens::CaptureHandle& capture) const
{
    for (const DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration) return true;
    }
    return false;
}

F32 LLPrismPreviewService::requestedHz(
    const LLPrismLens::CaptureRateSettings& rate)
{
    static LLCachedControl<F32> preview_fps(
        gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
    const F32 requested = LLPrismPreviewPolicy::requestedHz(
        preview_fps, rate.mMode, rate.mTargetFps);
    if (mBudget.warming())
    {
        if (mWarmupGrantFrame != gFrameCount)
        {
            mWarmupGrantFrame = gFrameCount;
            mWarmupGrantUsed = false;
        }
        if (mWarmupGrantUsed) return 0.f;
        mWarmupGrantUsed = true;
    }
    return mBudget.effectiveRate(requested);
}

void LLPrismPreviewService::notePublication(
    const LLPrismLens::CaptureHandle& capture, U64 serial,
    bool preview_quality)
{
    for (DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration)
        {
            entry.mPublicationSerial = serial;
            entry.mPreviewQuality = preview_quality;
            if (preview_quality && mSelfTestPhase == ESelfTestPhase::ENABLED)
            {
                const U32 slot = LLPrismLens::captureSlot(capture);
                if (slot < mSelfTestProduced.size()) ++mSelfTestProduced[slot];
            }
            markChanged();
            return;
        }
    }
}

bool LLPrismPreviewService::hasPendingAtlasUpdate() const
{
    static LLCachedControl<bool> previews_enabled(
        gSavedSettings, "PrismMonitorPreviewsEnabled", true);
    if (!previews_enabled || !anyVisibleConsumer() || !mBudget.canAttempt()) return false;
    const F64 now = LLTimer::getTotalSeconds();
    for (U32 slot = 0; slot < mDemand.size(); ++slot)
    {
        const DemandEntry& entry = mDemand[slot];
        if (entry.mDemanded && entry.mPublicationSerial > entry.mAtlasSerial)
            if (now >= mNextAtlasDue[slot]) return true;
    }
    return false;
}

namespace
{
constexpr U32 ATLAS_COLUMNS = 4;
constexpr U32 ATLAS_ROWS = 2;
constexpr U32 CELL_WIDTH = 256;
constexpr U32 CELL_HEIGHT = 144;
constexpr U32 ATLAS_WIDTH = ATLAS_COLUMNS * CELL_WIDTH;
constexpr U32 ATLAS_HEIGHT = ATLAS_ROWS * CELL_HEIGHT;

struct ScopedPreviewAtlasGL
{
    ScopedPreviewAtlasGL()
        : mShader(LLGLSLShader::sCurBoundShaderPtr),
          mCamera(LLViewerCamera::sCurCameraID), mPipelineRT(gPipeline.mRT),
          mScissor(glIsEnabled(GL_SCISSOR_TEST)),
          mBlend(glIsEnabled(GL_BLEND)), mDepth(glIsEnabled(GL_DEPTH_TEST)),
          mFramebufferSrgb(glIsEnabled(GL_FRAMEBUFFER_SRGB)),
          mActiveTextureUnit(gGL.getCurrentTexUnitIndex()),
          mTexture0(gGL.getTexUnit(0)->getCurrTexture()),
          mTexture0Type(gGL.getTexUnit(0)->getCurrType())
    {
        std::memcpy(mViewport, gGLViewport, sizeof(mViewport));
        glGetIntegerv(GL_SCISSOR_BOX, mScissorBox);
        glGetIntegerv(GL_BLEND_SRC_RGB, &mBlendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &mBlendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mBlendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mBlendDstAlpha);
        glGetIntegerv(GL_DEPTH_FUNC, &mDepthFunc);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, mColorMask);
        gGL.getTexUnit(0)->activate();
    }
    ~ScopedPreviewAtlasGL()
    {
        LLTexUnit* texture0 = gGL.getTexUnit(0);
        if (mTexture0 != 0)
        {
            if (texture0->getCurrTexture() != mTexture0 ||
                texture0->getCurrType() != mTexture0Type)
            {
                if (texture0->getCurrType() != LLTexUnit::TT_NONE)
                    texture0->unbind(texture0->getCurrType());
                texture0->bindManual(mTexture0Type, mTexture0);
            }
        }
        else if (texture0->getCurrType() != LLTexUnit::TT_NONE)
        {
            texture0->unbind(texture0->getCurrType());
        }
        gGL.getTexUnit(mActiveTextureUnit)->activate();
        if (mShader && mShader->isComplete()) mShader->bind();
        else LLGLSLShader::unbind();
        setEnabled(GL_SCISSOR_TEST, mScissor);
        glScissor(mScissorBox[0], mScissorBox[1], mScissorBox[2], mScissorBox[3]);
        setEnabled(GL_BLEND, mBlend);
        glBlendFuncSeparate(mBlendSrcRgb, mBlendDstRgb,
                            mBlendSrcAlpha, mBlendDstAlpha);
        setEnabled(GL_DEPTH_TEST, mDepth);
        glDepthFunc(mDepthFunc);
        glDepthMask(mDepthMask);
        gGL.setColorMask(mColorMask[0] != GL_FALSE, mColorMask[1] != GL_FALSE,
                         mColorMask[2] != GL_FALSE, mColorMask[3] != GL_FALSE);
        setEnabled(GL_FRAMEBUFFER_SRGB, mFramebufferSrgb);
        std::memcpy(gGLViewport, mViewport, sizeof(mViewport));
        glViewport(mViewport[0], mViewport[1], mViewport[2], mViewport[3]);
        LLViewerCamera::sCurCameraID = mCamera;
        gPipeline.mRT = mPipelineRT;
    }
    static void setEnabled(GLenum cap, GLboolean enabled)
    {
        if (enabled) glEnable(cap); else glDisable(cap);
    }
    LLGLSLShader* mShader;
    LLViewerCamera::eCameraID mCamera;
    LLPipeline::RenderTargetPack* mPipelineRT;
    S32 mViewport[4];
    GLboolean mScissor, mBlend, mDepth, mFramebufferSrgb;
    GLint mScissorBox[4], mBlendSrcRgb, mBlendDstRgb;
    GLint mBlendSrcAlpha, mBlendDstAlpha, mDepthFunc;
    GLboolean mDepthMask, mColorMask[4];
    U32 mActiveTextureUnit, mTexture0;
    LLTexUnit::eTextureType mTexture0Type;
};

void uploadSignalUniforms(LLGLSLShader& shader,
                          const LLPrismLens::CompositeState& state,
                          F32 effect_time)
{
    static const LLStaticHashedString sScale("displayToCaptureScale");
    static const LLStaticHashedString sOffset("displayToCaptureOffset");
    static const LLStaticHashedString sOrientationScale("retainedOrientationScale");
    static const LLStaticHashedString sOrientationOffset("retainedOrientationOffset");
    static const LLStaticHashedString sRegionScale("textureRegionScale");
    static const LLStaticHashedString sRegionOffset("textureRegionOffset");
    static const LLStaticHashedString sLetterbox("letterbox");
    static const LLStaticHashedString sBars("barColorLinear");
    static const LLStaticHashedString sFeather("edgeFeather");
    static const LLStaticHashedString sOptics("prismLensOptics");
    static const LLStaticHashedString sEffect0("screenEffect0");
    static const LLStaticHashedString sEffect1("screenEffect1");
    static const LLStaticHashedString sEffect2("screenEffect2");
    static const LLStaticHashedString sEffect3("screenEffect3");
    static const LLStaticHashedString sEffect4("screenEffect4");
    static const LLStaticHashedString sEffect5("screenEffect5");
    static const LLStaticHashedString sTime("screenEffectTime");
    shader.uniform2fv(sScale, 1, state.mDisplayToCaptureScale);
    shader.uniform2fv(sOffset, 1, state.mDisplayToCaptureOffset);
    shader.uniform2fv(sOrientationScale, 1, state.mRetainedOrientationScale);
    shader.uniform2fv(sOrientationOffset, 1, state.mRetainedOrientationOffset);
    shader.uniform2fv(sRegionScale, 1, state.mTextureRegionScale);
    shader.uniform2fv(sRegionOffset, 1, state.mTextureRegionOffset);
    shader.uniform1i(sLetterbox, state.mLetterbox);
    shader.uniform3fv(sBars, 1, state.mBarColorLinear);
    shader.uniform1f(sFeather, state.mEdgeFeather);
    shader.uniform4fv(sOptics, 1, state.mOpticsParams);
    shader.uniform4fv(sEffect0, 1, state.mScreenEffect0);
    shader.uniform4fv(sEffect1, 1, state.mScreenEffect1);
    shader.uniform4fv(sEffect2, 1, state.mScreenEffect2);
    shader.uniform4fv(sEffect3, 1, state.mScreenEffect3);
    shader.uniform4fv(sEffect4, 1, state.mScreenEffect4);
    shader.uniform4fv(sEffect5, 1, state.mScreenEffect5);
    shader.uniform1f(sTime, effect_time);
}
} // namespace

void LLPrismPreviewService::updateAtlas()
{
    if (!hasPendingAtlasUpdate()) return; // Feature-off: no GL query or mutation.
    if (mLastAtlasFrame == gFrameCount) return;
    mLastAtlasFrame = gFrameCount;
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism preview atlas composite");

    U32 cell = static_cast<U32>(mDemand.size());
    const F64 now = LLTimer::getTotalSeconds();
    for (U32 offset = 0; offset < mDemand.size(); ++offset)
    {
        const U32 candidate = (mNextAtlasSlot + offset) % mDemand.size();
        const DemandEntry& entry = mDemand[candidate];
        if (entry.mDemanded && entry.mPublicationSerial > entry.mAtlasSerial &&
            now >= mNextAtlasDue[candidate])
        {
            cell = candidate;
            break;
        }
    }
    if (cell >= mDemand.size()) return;

    DemandEntry& entry = mDemand[cell];
    static LLCachedControl<F32> preview_fps(
        gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
    LLPrismPreviewOperation budget_operation(
        PrismPreviewBudgetController::EOperation::ATLAS, entry.mCapture);
    if (!budget_operation.admitted())
    {
        const F32 effective_fps = llmax(0.5f, mBudget.effectiveRate(preview_fps));
        mNextAtlasDue[cell] = now + 1.0 / static_cast<F64>(effective_fps);
        return;
    }
    LL_PROFILE_GPU_ZONE("Prism preview atlas composite");
    const F32 effective_fps = mBudget.effectiveRate(preview_fps);
    mNextAtlasDue[cell] = now +
        1.0 / llclamp(static_cast<F64>(effective_fps), 0.5, 5.0);
    LLPrismLens::CompositeState state;
    bool offline = false;
    if (!LLPrismLens::buildPreviewCompositeState(
            entry.mCapture, mSelection[cell],
            static_cast<F32>(CELL_WIDTH) / static_cast<F32>(CELL_HEIGHT),
            state, offline))
    {
        TilePublication& tile = mTiles[cell];
        tile.mState = ETileState::OFFLINE;
        tile.mTimestamp = LLTimer::getTotalSeconds();
        tile.mPublicationSerial = entry.mPublicationSerial;
        entry.mAtlasSerial = entry.mPublicationSerial;
        markChanged();
        mNextAtlasSlot = (cell + 1) % mDemand.size();
        return;
    }

    const U32 permutation = gPipeline.colorCorrectPermutation(true, true);
    LLGLSLShader& shader = gPrismPreviewProgram[permutation];
    if (!shader.isComplete())
    {
        mTiles[cell].mState = ETileState::SHADER_UNAVAILABLE;
        mTiles[cell].mTimestamp = LLTimer::getTotalSeconds();
        mTiles[cell].mPublicationSerial = entry.mPublicationSerial;
        entry.mAtlasSerial = entry.mPublicationSerial;
        mNextAtlasSlot = (cell + 1) % mDemand.size();
        markChanged();
        return;
    }

    ScopedPreviewAtlasGL restore;
    if (!mAtlas.isComplete())
    {
        // Shader writes explicit display-sRGB code values, so use non-decoding
        // RGBA8 storage; LLUI consumes those values as display pixels.
        if (!mAtlas.allocate(ATLAS_WIDTH, ATLAS_HEIGHT, GL_RGBA8))
        {
            mTiles[cell].mState = ETileState::FAILED;
            markChanged();
            return;
        }
        ++mAtlasGeneration;
    }

    const U32 column = cell % ATLAS_COLUMNS;
    const U32 row = cell / ATLAS_COLUMNS;
    const S32 x = static_cast<S32>(column * CELL_WIDTH);
    const S32 y = static_cast<S32>((ATLAS_ROWS - 1u - row) * CELL_HEIGHT);
    mAtlas.bindTarget();
    glViewport(x, y, CELL_WIDTH, CELL_HEIGHT);
    const S32 atlas_viewport[4] = {
        x, y, static_cast<S32>(CELL_WIDTH), static_cast<S32>(CELL_HEIGHT) };
    std::memcpy(gGLViewport, atlas_viewport, sizeof(gGLViewport));
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, CELL_WIDTH, CELL_HEIGHT);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    gGL.setColorMask(true, true, true, true);
    glDisable(GL_FRAMEBUFFER_SRGB);

    shader.bind();
    const S32 signal_channel = shader.enableTexture(LLShaderMgr::PRISM_LENS_MAP);
    if (signal_channel >= 0)
    {
        gPipeline.mPrismLensOutput[state.mCaptureSlot].bindTexture(
            0, signal_channel, LLTexUnit::TFO_BILINEAR);
    }
    const S32 exposure_channel = shader.enableTexture(LLShaderMgr::EXPOSURE_MAP);
    if (exposure_channel >= 0)
    {
        gPipeline.mExposureMap.bindTexture(0, exposure_channel, LLTexUnit::TFO_POINT);
    }
    uploadSignalUniforms(shader, state, fmodf(gFrameTimeSeconds, 3600.f));
    static const LLStaticHashedString sSheenSky("prismEnvSheenSky");
    static const LLStaticHashedString sSheenGround("prismEnvSheenGround");
    LLColor3 sheen_sky(0.f, 0.f, 0.f);
    LLColor3 sheen_ground(0.f, 0.f, 0.f);
    if (const LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky())
    {
        const LLColor3 ambient = sky->getAmbientColor();
        sheen_sky = ambient * 1.5f + sky->getBlueHorizon() * 0.4f;
        sheen_ground = ambient * 0.5f;
    }
    shader.uniform3fv(sSheenSky, 1, sheen_sky.mV);
    shader.uniform3fv(sSheenGround, 1, sheen_ground.mV);
    static const LLStaticHashedString sWorldOrigin("previewWorldOrigin");
    static const LLStaticHashedString sWorldU("previewWorldUEdge");
    static const LLStaticHashedString sWorldV("previewWorldVEdge");
    const LLVector3 world_origin(state.mVirtualCorners[0]);
    const LLVector3 world_u = LLVector3(state.mVirtualCorners[1]) - world_origin;
    const LLVector3 world_v = LLVector3(state.mVirtualCorners[3]) - world_origin;
    shader.uniform3fv(sWorldOrigin, 1, world_origin.mV);
    shader.uniform3fv(sWorldU, 1, world_u.mV);
    shader.uniform3fv(sWorldV, 1, world_v.mV);
    gPipeline.uploadPrismPreviewColorCorrectUniforms(shader);
    const S32 color_grade_channel =
        gPipeline.uploadPrismPreviewColorGradeUniforms(shader);

    LLVertexBuffer::unbind();
    gGL.begin(LLRender::TRIANGLES);
    gGL.vertex3f(-1.f, -1.f, 0.f);
    gGL.vertex3f(3.f, -1.f, 0.f);
    gGL.vertex3f(-1.f, 3.f, 0.f);
    gGL.end();
    shader.disableTexture(LLShaderMgr::EXPOSURE_MAP);
    shader.disableTexture(LLShaderMgr::PRISM_LENS_MAP);
    if (color_grade_channel >= 0)
        gGL.getTexUnit(color_grade_channel)->unbind(LLTexUnit::TT_TEXTURE_3D);
    mAtlas.flush();

    entry.mAtlasSerial = entry.mPublicationSerial;
    TilePublication& tile = mTiles[cell];
    tile.mAtlas = atlasHandle();
    const F32 left = static_cast<F32>(column) / ATLAS_COLUMNS;
    const F32 right = static_cast<F32>(column + 1u) / ATLAS_COLUMNS;
    const F32 top = 1.f - static_cast<F32>(row) / ATLAS_ROWS;
    const F32 bottom = 1.f - static_cast<F32>(row + 1u) / ATLAS_ROWS;
    tile.mCellUV = LLRectf(left, top, right, bottom);
    LLPrismLens::EActivityState activity = LLPrismLens::EActivityState::IDLE;
    bool healthy = false;
    LLPrismLens::previewTileActivity(entry.mCapture, activity, healthy);
    if (!healthy) tile.mState = ETileState::OFFLINE;
    else if (activity == LLPrismLens::EActivityState::PAUSED)
        tile.mState = ETileState::PAUSED;
    else if (activity == LLPrismLens::EActivityState::THROTTLED)
        tile.mState = ETileState::THROTTLED;
    else tile.mState = ETileState::READY;
    tile.mTimestamp = LLTimer::getTotalSeconds();
    tile.mPublicationSerial = entry.mPublicationSerial;
    markChanged();
    mNextAtlasSlot = (cell + 1) % mDemand.size();
}

void LLPrismPreviewService::onRenderTargetsReleased()
{
    // Lens publication serials restart at zero in the paired registry callback.
    // Rebase both service serials to the same origin and invalidate every tile,
    // so publication 1 is immediately eligible in the new GL generation.
    for (U32 slot = 0; slot < mDemand.size(); ++slot)
    {
        DemandEntry& entry = mDemand[slot];
        entry.mPublicationSerial = 0;
        entry.mAtlasSerial = 0;
        entry.mPreviewQuality = false;
        mNextAtlasDue[slot] = 0.0;
        TilePublication& tile = mTiles[slot];
        tile.mAtlas = AtlasHandle{};
        tile.mState = ETileState::EMPTY;
        tile.mPublicationSerial = 0;
    }
    mAtlas.release();
    ++mAtlasGeneration;
    markChanged();
    mScratchRetryAfter = 0.0;
    mScratchWarmed = false;
    for (QueryEntry& query : mQueries)
    {
        if (query.mPending && query.mBudgetGeneration == mBudgetGeneration)
            mBudget.complete(query.mOperation, query.mReservationMs,
                             query.mCpuMs, 0.0, false);
        if (gGLManager.mInited && glDeleteQueries)
        {
            if (query.mStart != 0) glDeleteQueries(1, &query.mStart);
            if (query.mEnd != 0) glDeleteQueries(1, &query.mEnd);
        }
        // With no live context the old namespace is already gone; only then is
        // forgetting the names correct.
        query = QueryEntry{};
    }
    mBudget.reset();
    ++mBudgetGeneration;
}

ScopedPrismPreviewQuality::ScopedPrismPreviewQuality()
    : mShadowDetail(LLPipeline::RenderShadowDetail),
      mDeferredSSAO(LLPipeline::RenderDeferredSSAO),
      mVolumetricLighting(LLPipeline::RenderVolumetricLighting),
      mProjectorVolumetrics(LLPipeline::BDMergeProjectorVolumetrics),
      mFarClip(LLPipeline::RenderFarClip),
      mPreviewRender(LLPipeline::sPrismPreviewRender)
{
    LLPipeline::sPrismPreviewRender = true;
    LLPipeline::RenderShadowDetail = 0;
    LLPipeline::RenderDeferredSSAO = false;
    LLPipeline::RenderVolumetricLighting = false;
    LLPipeline::BDMergeProjectorVolumetrics = false;
    LLPipeline::RenderFarClip = llmin(mFarClip, LLPrismPreviewPolicy::MAX_FAR_CLIP);
}

ScopedPrismPreviewQuality::~ScopedPrismPreviewQuality()
{
    LLPipeline::RenderFarClip = mFarClip;
    LLPipeline::BDMergeProjectorVolumetrics = mProjectorVolumetrics;
    LLPipeline::RenderVolumetricLighting = mVolumetricLighting;
    LLPipeline::RenderDeferredSSAO = mDeferredSSAO;
    LLPipeline::RenderShadowDetail = mShadowDetail;
    LLPipeline::sPrismPreviewRender = mPreviewRender;
}
