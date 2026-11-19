/**
 * @file alpanelpracticals.h
 * @brief Director > Light > Practicals page logic: shadows, light cone and gobo for the selected projector.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The page root of the generated kit page (class "panel_practicals"). It does for the Director page
 * what the "Selected Light" tab of the Lightbox does (ALFloaterLightBox::updateSelectedLightPanel,
 * onSelLight* and onSelGoboChanged): it follows the primary selection, enables the per-projector
 * controls only for a spotlight projector, mirrors the session-only pipeline state into them, and
 * writes edits back through the same LLPipeline entry points. Nothing here is persisted.
 */

#ifndef AL_ALPANELPRACTICALS_H
#define AL_ALPANELPRACTICALS_H

#include "aluirefresh.h"
#include "alvirtuallights.h"
#include "llpanel.h"
#include "lluuid.h"

#include <functional>

class LLContextMenu;
class LLScrollListCtrl;
class LLSpinCtrl;
class LLTextureCtrl;
class LLViewerObject;
class LLVOVolume;
class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLSliderCtrl;
class LLTextBox;

// The "Virtual lights" card of the Practicals page: place, list, edit, delete and the per-light
// look/position/aim/VCam controls. Session-only model in ALVirtualLights; every edit is one
// undo step on the Practicals page (a burst of edits coalesces into one).
class ALVirtualLightsUI
{
public:
    ALVirtualLightsUI() = default;
    // Teardown DISCARDS a pending edit (no undo recording while the page is being destroyed).
    ~ALVirtualLightsUI();
    void attach(LLPanel* root);
    // Page hidden: the pending edit becomes its undo step now; the place / edit tools and the marker end.
    void onHide();
    // From the page draw(): cheap checks only, throttled mirror writes.
    void tick();
    void onReveal() { mForce = true; }
    // A Practicals-card edit (cast shadows, gobo, track, ...) aimed at a virtual light.
    void practicalsEdit(const LLUUID& key, const char* label, LLUICtrl* control, const std::function<void()>& fn);
    static void finishAllPending();

private:
    void refresh(bool force);
    void edit(const char* label, LLUICtrl* control, const std::function<void(ALVirtualLights::Light&)>& change);
    void editKey(const LLUUID& key, const char* label, LLUICtrl* control,
                 const std::function<void(ALVirtualLights::Light&)>& change);
    // Closes the pending step unless `control` continues the same gesture; returns the "before" snapshot the
    // caller hands to notePending (undefined while a gesture continues).
    LLSD beginEdit(LLUICtrl* control);
    void notePending(const LLSD& before, const char* label, LLUICtrl* control);
    void finishPending(bool force);
    void onTexturePick();
    void onTexturePicked();
    void onPlace();
    void onAimPick();
    void onDelete();
    void onEditToggle();
    void onVCam();
    void setMessage(const std::string& text);
    LLPanel* mRoot = nullptr;
    LLButton* mPlace = nullptr;
    LLButton* mDelete = nullptr;
    LLButton* mAimPick = nullptr;
    LLCheckBoxCtrl* mEditToggle = nullptr;
    LLCheckBoxCtrl* mOn = nullptr;
    LLScrollListCtrl* mList = nullptr;
    LLTextBox* mStatus = nullptr;
    LLComboBox* mType = nullptr;
    LLComboBox* mVCam = nullptr;
    LLButton* mTexture = nullptr;
    LLTextBox* mTextureName = nullptr;
    LLTextureCtrl* mTexturePicker = nullptr;   // hidden: opens the viewer's standard texture picker floater
    LLUUID mTextureKey;                        // the light the picker was opened for
    LLUUID mNamedTexture;
    std::string mNamedTextureName;
    LLColorSwatchCtrl* mColor = nullptr;
    LLSliderCtrl* mIntensity = nullptr;
    LLSliderCtrl* mRadius = nullptr;
    LLSliderCtrl* mFalloff = nullptr;
    LLSliderCtrl* mFov = nullptr;
    LLSliderCtrl* mFocus = nullptr;
    LLSliderCtrl* mAmbiance = nullptr;
    LLSpinCtrl* mX = nullptr;
    LLSpinCtrl* mY = nullptr;
    LLSpinCtrl* mZ = nullptr;
    LLSpinCtrl* mYaw = nullptr;
    LLSpinCtrl* mPitch = nullptr;
    ALLiveReadoutClock mClock{10.f};
    U64 mRevision = 0;
    U64 mPrismRevision = 0;
    LLUUID mShownFocus;
    bool mForce = true;
    std::string mMessage;
    F64 mMessageTime = 0.0;
    // Gesture coalescing: one undo step per burst of edits.
    LLSD mPendingBefore;
    std::string mPendingLabel;
    U64 mPendingEpoch = 0;
    F64 mLastEdit = 0.0;
    LLUICtrl* mPendingControl = nullptr;
};

class ALPanelPracticals final : public LLPanel
{
public:
    ALPanelPracticals() = default;
    ~ALPanelPracticals() override;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;

private:
    // What the page edits: a virtual light when one has the Practicals focus, else the selection.
    struct Target
    {
        LLViewerObject* mObject = nullptr;
        LLVOVolume*     mVolume = nullptr;
        LLUUID          mId;
        LLUUID          mKey;     // virtual light key, null for a real object
        bool            mVirtual = false;
    };
    Target resolveTarget() const;
    // The Track card's state for either backend (the real tracker, or a virtual light's own).
    struct TrackView
    {
        bool eligible = false;
        bool enabled = false;
        bool hasSubject = false;
        LLUUID subject;
        S32 joint = 0;
        S32 mode = 0;
        F32 smoothing = 0.15f;
        std::string status;
    };
    TrackView trackView(const Target& target) const;
    void configureTrackFor(const Target& target, const LLUUID& subject, bool has_subject,
                           S32 joint, S32 mode, F32 smoothing);
    // One undo step per edit when the target is a virtual light; a plain call otherwise.
    void edited(const Target& target, const char* label, LLUICtrl* control, const std::function<void()>& fn);
    // The Track picker is asynchronous: it captures WHICH target it was opened for (kind + stable id) and, on
    // completion, resolves and validates exactly that one (a vanished target means nothing happens).
    ALVirtualLightsModel::TrackTargetRef trackTargetNow() const;
    void refreshFromSelection();
    void onCastShadows();
    void onVolumetric();
    void onCaptureOverride();
    void onClearOverride();
    void onGoboChanged(LLUICtrl* source);
    void refreshTracking(bool force);
    void onTrackingChanged(LLUICtrl* source);
    void assignTrackingSubject(const ALVirtualLightsModel::TrackTargetRef& target, const LLUUID& subject,
                               bool has_subject);
    LLCheckBoxCtrl* mTrack = nullptr;
    LLComboBox* mTrackJoint = nullptr;
    LLComboBox* mTrackMode = nullptr;
    LLSliderCtrl* mTrackSmoothing = nullptr;
    LLButton* mTrackCapture = nullptr;
    LLButton* mTrackChoose = nullptr;
    LLButton* mTrackSelected = nullptr;
    LLButton* mTrackYou = nullptr;
    LLButton* mTrackClear = nullptr;
    LLTextBox* mTrackSubject = nullptr;
    LLTextBox* mTrackStatus = nullptr;
    LLHandle<LLContextMenu> mTrackMenu;
    U64 mTrackRevision = 0;
    U64 mTrackCastRevision = 0;
    U64 mTrackVCamRevision = 0;
    U64 mTrackVirtualRevision = 0;

    LLCheckBoxCtrl*    mCastShadows = nullptr;
    LLCheckBoxCtrl*    mVolumetric = nullptr;
    LLButton*          mCapture = nullptr;
    LLButton*          mClear = nullptr;
    LLTextBox*         mStatus = nullptr;
    LLComboBox*        mGoboPattern = nullptr;
    LLComboBox*        mGoboAnim = nullptr;
    LLSliderCtrl*      mGoboSpeed = nullptr;
    LLSliderCtrl*      mGoboZoom = nullptr;
    LLSliderCtrl*      mGoboDispersion = nullptr;
    LLColorSwatchCtrl* mGoboTint = nullptr;
    LLSliderCtrl*      mGoboVar1 = nullptr;
    LLSliderCtrl*      mGoboVar2 = nullptr;
    LLSliderCtrl*      mGoboSoft = nullptr;
    LLCheckBoxCtrl*    mGoboInvert = nullptr;

    LLUUID mLastId;
    bool   mLastProjector = false;
    ALVirtualLightsUI mVirtualUI;
};

#endif // AL_ALPANELPRACTICALS_H
