/**
 * @file aldirectorswitcher.h
 * @brief Viewer adapter for the Director's deterministic camera switcher.
 *
 * The renderer-independent schedule lives in aldirectorswitchermodel.*. This
 * adapter owns settings/bank serialization and exposes one effective CineCam
 * mode at a time. It never writes UseCinematicCamera.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALDIRECTORSWITCHER_H
#define AL_ALDIRECTORSWITCHER_H

#include "aldirectorswitchermodel.h"
#include "stdtypes.h"
#include "lltimer.h"

#include <string>
#include <vector>

class ALDirectorSwitcher
{
public:
    static constexpr S32 SLOT_COUNT =
        ALDirectorSwitcherModel::SLOT_COUNT;

    enum ESubjectTarget : S32
    {
        SUBJECT_DEFAULT = 0,
        SUBJECT_A = 1,
        SUBJECT_B = 2,
        SUBJECT_C = 3,
        SUBJECT_D = 4,
    };

    struct Slot
    {
        bool        mEnabled = true; // auto eligibility; manual punch ignores it
        S32         mMode = 0;
        std::string mLabel;
        // Primary Default preserves CineCam's A/follow/selected/self chain.
        // Secondary B preserves legacy A-over-B OTS/two-shot behavior.
        S32 mPrimarySubject = SUBJECT_DEFAULT;
        S32 mSecondarySubject = SUBJECT_B;
        // Optional subject-relative static rig. Motion modes ignore it;
        // Capture Current View assigns a static mode before enabling it.
        bool mCustomEnabled = false;
        F32  mCustomYawOffsetDeg = 0.f;
        F32  mCustomPitchDeg = 0.f;
        F32  mCustomDistanceM = 3.f;
        F32  mCustomHeightM = 1.3f;
        F32  mCustomFovDeg = 60.f;
    };

    static ALDirectorSwitcher& instance();

    // Called once per viewer frame after presentation time is frozen and just
    // before top-level camera dispatch.
    void tick(F64 presentation_time);

    // Release a live eased-cut world-time override. Safe and idempotent; the
    // Director Console calls this when its switcher panel closes.
    void cancelEaseWorldTime();

    // Manual program punch. A disabled slot remains manually selectable.
    // Returns true only when a real cut was issued.
    bool punch(S32 slot);

    // Director ACTION/CUT cooperation. DirectorArmCamera remains the gate.
    void onDirectorAction();
    void onDirectorCut();

    bool isDrivingCamera() const;
    S32  activeSlot() const       { return mActiveSlot; }
    S32  activeMode() const       { return mActiveMode; }
    U64  cutSerial() const        { return mCutSerial; }
    F64  activeSince() const      { return mActiveSince; }
    F32  cutEaseSeconds() const   { return mCutEaseSeconds; }
    S32  cutEaseCurve() const     { return mCutEaseCurveId; }
    const F32* cutEaseBezier() const { return mCutEaseBezier; }
    F32  cutEaseFeather() const   { return mCutEaseFeather; }
    F64  cutEaseElapsedSeconds() const
    {
        return mCutEaseTimer.getElapsedTimeF64();
    }
    bool activeCustomAngle(Slot& slot) const;
    S32  activePrimarySubject() const;
    S32  activeSecondarySubject() const;

    // Supplies the operator-owned camera-enable value hidden under this
    // switcher's live override. Director ACTION uses it for a symmetric lease
    // handoff when the switcher was armed first.
    bool cameraEnableBaseline(bool& enabled) const;

    static std::vector<Slot> loadBank();
    static void saveBank(const std::vector<Slot>& bank);

private:
    ALDirectorSwitcher() = default;

    static std::vector<Slot> defaultBank();
    static void sanitizeSlot(Slot& slot);
    static S32 sanitizeMode(S32 mode);
    static S32 sanitizePrimarySubject(S32 subject);
    static S32 sanitizeSecondarySubject(S32 subject);
    static ALDirectorSwitcherModel::Config readConfig(
        const std::vector<Slot>& bank);

    void enterArmed(F64 now, const std::vector<Slot>& bank);
    void leaveArmed();
    void startEaseWorldTime(F64 now);
    void updateEaseWorldTime(F64 now);
    void applyEaseWorldScale(F32 scale);
    void restoreEaseWorldTime();
    void writeCameraEnabled(bool enabled);
    bool shouldYieldToFlycam() const;
    bool applySlot(S32 slot, F64 now, F64 boundary,
                   const std::vector<Slot>& bank, bool manual,
                   bool force = false);

    ALDirectorSwitcherModel::Controller mController;

    bool mWasArmed = false;
    bool mWasCameraEnabled = false;
    bool mHaveEnableSnapshot = false;
    bool mEnableWasEnabled = false;
    bool mWroteEnable = false;
    bool mLastWrittenEnable = false;
    bool mYieldingToFlycam = false;

    S32 mActiveSlot = -1;
    S32 mActiveMode = 0;
    Slot mActiveSlotConfig;
    U64 mCutSerial = 0;
    F64 mActiveSince = 0.0;
    F32 mCutEaseSeconds = 0.f;
    S32 mCutEaseCurveId = 0;
    F32 mCutEaseBezier[4] = { 0.42f, 0.f, 0.58f, 1.f };
    F32 mCutEaseFeather = 0.f;
    LLTimer mCutEaseTimer;
    F64 mLastPresentationTime = 0.0;

    // Transient ownership of the existing Temporal Capture / Freeze World
    // controls during one eased cut. All values are restored exactly.
    bool mEaseWorldTimeActive = false;
    bool mEaseUsesFreezeWorld = false;
    S32  mSavedTemporalMode = 0;
    F32  mSavedTemporalWorldScale = 1.f;
    bool mSavedDriveAnimation = true;
    bool mSavedDriveObjects = true;
    bool mSavedDriveTextureAnim = true;
    bool mSavedDriveParticles = true;
    bool mSavedBDMergeFreezeWorld = false;
    bool mSavedUseFreezeWorld = false;
    bool mSavedFreezeTime = false;
    F32  mEaseBaseWorldScale = 1.f;
    F32  mEaseTargetFactor = 1.f;
    S32  mFreezeCurveId = 0;
    F32  mFreezeBezier[4] = { 0.42f, 0.f, 0.58f, 1.f };
    F32  mFreezeFeather = 0.f;
};

#endif // AL_ALDIRECTORSWITCHER_H
