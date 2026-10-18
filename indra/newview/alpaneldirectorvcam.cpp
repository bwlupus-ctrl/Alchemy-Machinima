/**
 * @file alpaneldirectorvcam.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "alpaneldirectorvcam.h"

#include "alkitcard.h"
#include "alkitstyle.h"
#include "alkitpage.h"
#include "alkitrow.h"
#include "alkitswitch.h"
#include "aldirectorcastui.h"
#include "alobjectproperties.h"
#include "aluirefresh.h"
#include "alvcambonepov.h"
#include "alvcammonitorstrip.h"
#include "alvcammediafileio.h"
#include "alvcammediafilelifecycle.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "lldirectorcast.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llradiogroup.h"
#include "llselectmgr.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewermenu.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "lltool.h"
#include "lltoolmgr.h"

#include <algorithm>
#include <cmath>

static LLPanelInjector<ALPanelDirectorVCam>
    t_panel_director_vcam("panel_director_vcam");

namespace
{
// A transient face picker; cancellation has no registry side effects. Capture
// handle/generation + revision are checked by the owning panel on completion.
class MediaFacePickTool final : public LLTool
{
public:
    MediaFacePickTool() : LLTool("VCam media face") {}
    static MediaFacePickTool& instance() { static MediaFacePickTool tool; return tool; }
    std::function<void(LLViewerObject*, S32)> mPicked;
    U64 mGeneration = 1;
    U64 mPendingGeneration = 0;
    bool mPickPending = false;
    void begin(std::function<void(LLViewerObject*, S32)> callback)
    { ++mGeneration; mPicked = std::move(callback); }
    bool handleMouseDown(S32 x, S32 y, MASK mask) override
    {
        // One outstanding callback, including across cancel/re-open. An old
        // result must not consume the callback installed by a newer picker.
        if (mPickPending) return true;
        mPickPending = true;
        mPendingGeneration = mGeneration;
        gViewerWindow->pickAsync(x, y, mask, [](const LLPickInfo& pick)
        {
            auto& tool = instance();
            tool.mPickPending = false;
            if (tool.mPendingGeneration != tool.mGeneration ||
                LLToolMgr::getInstance()->getCurrentTool() != &tool) return;
            const auto callback = std::move(tool.mPicked);
            LLToolMgr::getInstance()->clearTransientTool();
            if (callback) callback(pick.getObject(), pick.mObjectFace);
        });
        return true;
    }
    bool handleHover(S32, S32, MASK) override
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLPICKOBJECT3);
        return true;
    }
    void handleDeselect() override { ++mGeneration; mPicked = {}; }
};

constexpr LLPrismPreviewService::VisibilityOwner VCAM_PAGE_PREVIEW_OWNER = 2;
bool setRowControlVisible(ALKitRow* row, bool visible)
{
    return row && row->setControlVisible(visible);
}

bool sameCapture(const LLPrismLens::CaptureHandle& a,
                 const LLPrismLens::CaptureHandle& b)
{
    return a.mId == b.mId && a.mGeneration == b.mGeneration;
}

bool sameDisplay(const LLPrismLens::DisplayHandle& a,
                 const LLPrismLens::DisplayHandle& b)
{
    return a.mId == b.mId && a.mGeneration == b.mGeneration;
}

bool sameProjector(const LLPrismLens::ProjectorHandle& a,
                   const LLPrismLens::ProjectorHandle& b)
{
    return a.mId == b.mId && a.mGeneration == b.mGeneration;
}

const char* performanceText(LLPrismLens::EPerformanceState state)
{
    using State = LLPrismLens::EPerformanceState;
    switch (state)
    {
        case State::DISABLED: return "off";
        case State::LEARNING: return "learning";
        case State::STEADY: return "steady";
        case State::PROTECTING: return "protecting";
        case State::SUSPENDED: return "suspended";
        case State::TIMING_UNAVAILABLE: return "timing unavailable";
    }
    return "unknown";
}

std::string displayName(const LLPrismLens::DisplayDefinition& display, U32 index)
{
    return display.mSettings.mVirtual
        ? llformat("Virtual screen %u", index + 1)
        : llformat("Prim face %u", index + 1);
}

// Face name for lists that mix per-camera and Gate-subscribed faces.
std::string faceListName(const LLPrismLens::DisplayDefinition& display, U32 index)
{
    return display.mGateSubscribed
        ? displayName(display, index) + " \xC2\xB7 Gate"
        : displayName(display, index);
}

std::string projectorName(const LLPrismLens::ProjectorDefinition& projector)
{
    static const char* const RIG_ROLES[] = { "Key", "Fill", "Rim", "BG" };
    if (projector.mTarget == LLPrismLens::EProjectorTarget::RIG_LIGHT)
    {
        const char* role = projector.mRigLight < 4
            ? RIG_ROLES[projector.mRigLight] : "Light";
        return llformat("Rig %u %s", projector.mRigSlot + 1, role);
    }

    LLViewerObject* object = gObjectList.findObject(projector.mObjectId);
    if (!object) return "Light (waiting)";
    const ALObjectPropertiesCache::ServerProps* properties =
        ALObjectPropertiesCache::instance().get(projector.mObjectId);
    if (properties && !properties->mName.empty()) return properties->mName;
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(object);
    if (node && !node->mName.empty()) return node->mName;
    const LLVector3 position = object->getPositionRegion();
    return llformat("Light at %.1f, %.1f, %.1f", position.mV[VX],
                    position.mV[VY], position.mV[VZ]);
}

const char* projectorState(const LLPrismLens::ProjectorDefinition& projector)
{
    if (projector.mRuntime.mHealth != LLPrismLens::EDisplayHealth::READY)
        return "Waiting";
    return projector.mLocalProjector ? "Local projector" : "Live";
}

F32 ratioFromValue(const LLSD& value, F32 fallback)
{
    const F32 ratio = static_cast<F32>(value.asReal());
    return ratio > 0.f ? ratio : fallback;
}

}

ALPanelDirectorVCam::ALPanelDirectorVCam() = default;

ALPanelDirectorVCam::~ALPanelDirectorVCam()
{
    LLPrismPreviewService::instance().clearDemand(VCAM_PAGE_PREVIEW_OWNER);
    LLPrismPreviewService::instance().releaseConsumer(VCAM_PAGE_PREVIEW_OWNER);
    if (LLContextMenu* menu = mMonitorMenu.get()) menu->die();
}

void ALPanelDirectorVCam::collectControls(LLView* root)
{
    if (!root) return;
    if (ALKitCard* card = dynamic_cast<ALKitCard*>(root))
        mCards.push_back(card);
    if (ALKitRow* kit_row = dynamic_cast<ALKitRow*>(root))
        mRows[kit_row->meta().widget] = kit_row;
    if (LLUICtrl* control_ptr = dynamic_cast<LLUICtrl*>(root))
    {
        const std::string& name = control_ptr->getName();
        if (name.rfind("vc_", 0) == 0)
            mControls[name] = control_ptr;
    }
    for (LLView* child : *root->getChildList())
        collectControls(child);
}

bool ALPanelDirectorVCam::postBuild()
{
    if (!LLPanel::postBuild()) return false;
    mKitPage = findChild<ALKitPage>("vcam_kit");
    collectControls(this);
    auto refresh_bone_anchor_choices = [this]()
    {
        const U64 revision = LLDirectorCast::instance().getActorChoiceRevision();
        if (revision == mBoneAnchorSubjectRevision)
            return;
        mBoneAnchorSubjectRevision = revision;
        ALDirectorCastUI::mergeSubjectCombo(
            dynamic_cast<LLComboBox*>(control("vc_bone_anchor")), 0, 1, 4,
            [this]() -> LLSD
            {
                const LLPrismLens::CaptureDefinition* capture = selectedCapture();
                return capture && capture->cameraData() ? LLSD(static_cast<S32>(
                                     capture->cameraData()->mSettings.mBonePov.mAnchorSlot))
                               : LLSD(0);
            });
    };
    refresh_bone_anchor_choices();
    static const char* const projector_control_names[PROJ_CONTROL_COUNT] = {
        "vc_proj_look", "vc_proj_fit", "vc_proj_anchor_x", "vc_proj_anchor_y",
        "vc_proj_flip_h", "vc_proj_flip_v", "vc_proj_rotate",
        "vc_proj_keystone_h", "vc_proj_keystone_v", "vc_proj_throw",
        "vc_proj_brightness", "vc_proj_lamp_power", "vc_proj_follow_exposure",
        "vc_proj_lamp_kelvin",
        "vc_proj_gel_tint", "vc_proj_hotspot", "vc_proj_shutter_flicker",
        "vc_proj_shutter_rate", "vc_proj_gate_weave", "vc_proj_grain",
        "vc_proj_dust", "vc_proj_scratches", "vc_proj_burn", "vc_proj_halation",
        "vc_proj_resolution", "vc_proj_no_signal", "vc_proj_keep_gobo",
        "vc_proj_tv_preset", "vc_proj_tv_scan", "vc_proj_tv_gray",
        "vc_proj_tv_static", "vc_proj_tv_vignette", "vc_proj_tv_brightness",
        "vc_proj_tv_lines", "vc_proj_tv_dark", "vc_proj_tv_sharp",
        "vc_proj_tv_thick", "vc_proj_tv_pixel", "vc_proj_tv_sepia",
        "vc_proj_tv_chroma", "vc_proj_tv_vroll", "vc_proj_tv_roll_speed",
        "vc_proj_tv_tracking", "vc_proj_tv_flicker", "vc_proj_tv_interlace",
        "vc_proj_tv_dropout", "vc_proj_tv_sheen"
    };
    for (U32 index = 0; index < PROJ_CONTROL_COUNT; ++index)
        mProjectorControls[index] = control(projector_control_names[index]);
    mMonitorStrip = findChild<ALVCamMonitorStrip>("vc_monitor_strip");
    mMonitorHeader = findChild<ALVCamMonitorHeader>("vc_monitor_header");
    mMonitorPager = findChild<ALVCamMonitorPager>("vc_monitor_pager");
    mCameraSelector = findChild<ALVCamCameraSelector>("vc_camera_selector");
    mFaceList = findChild<ALVCamFaceList>("vc_face_list");
    mProjectorList = findChild<ALVCamProjectorList>("vc_projector_list");
    mCameraName = dynamic_cast<LLLineEditor*>(control("vc_camera_name"));
    if (mCameraName)
        mCameraName->setMaxTextLength(128); // Core normalization caps at 32 Unicode characters.
    mMediaVideo = dynamic_cast<LLLineEditor*>(control("vc_media_video"));
    if (mMediaVideo)
    {
        mMediaVideo->setMaxTextLength(8192);
        mMediaVideo->setKeystrokeCallback([this](LLLineEditor*, void*)
        {
            ++mMediaEditEpoch; // Invalidates a pending Browse even if selection returns.
            validateMediaVideo();
        }, nullptr);
    }
    const auto media_panel = getHandle();
    doPeriodically([media_panel]()
    {
        auto* self = dynamic_cast<ALPanelDirectorVCam*>(media_panel.get());
        if (!self) return true;
        if (self->mModel.pageVisible() && self->isInVisibleChain())
        {
            self->refreshMediaCache();
        }
        return false;
    }, 0.5f);

    if (mKitPage)
    {
        setVisibleIfChanged(mKitPage->undoButton(), false);
        setVisibleIfChanged(mKitPage->redoButton(), false);
        setVisibleIfChanged(mKitPage->resetButton(), false);
        mKitPage->setRowRevealGuard([this](ALKitRow* target)
        {
            const std::string& widget = target->meta().widget;
            const bool url_row = widget == "vc_media_video" || widget == "vc_media_browse" ||
                widget == "vc_media_transport" || widget == "vc_media_loop" || widget == "vc_media_volume" ||
                widget == "vc_media_cache_clear" || widget == "vc_media_cache_status" || widget == "vc_media_cache_keep";
            const bool face_row = widget == "vc_media_face" || widget == "vc_media_face_actions";
            if (!url_row && !face_row) return true;
            const auto* selected = selectedCapture();
            if (selected && ((url_row && selected->mediaUrlData()) || (face_row && selected->mediaData()))) return true;
            setStatus(url_row ? "Select a media slot and switch Source to Video URL to use this control." :
                "Select a media slot and switch Source to Prim face to use this control.", false);
            return false;
        });
        mKitPage->setCardPasteHandler(
            [this](const std::string& card, const LLSD& values)
            { return pasteCardValues(card, values); });
    }
    for (auto& entry : mControls)
    {
        // A generated bind="setting" control commits directly to
        // gSavedSettings.  Do not also install the code router: this keeps the
        // row's sole write sink explicit for the fail-closed functional run.
        if (entry.second->getControlVariable())
            continue;
        entry.second->setCommitCallback(
            [this](LLUICtrl* control_ptr, const LLSD&) { onControl(control_ptr); });
    }

    const std::pair<const char*, const char*> info_rows[] = {
        {"vc_source_note_object", "Object cameras follow the selected camera object."},
        {"vc_source_note_virtual", "Virtual cameras store a position and rotation and need no prim."},
        {"vc_source_note_skeleton", "Skeleton POV follows a cast subject or you."},
        {"vc_lens_type", "Surface Lens"},
        {"vc_lens_note", "A Lens owns exactly one aperture face. Remove the capture to remove that face."},
        {"vc_place_eye_front", "Info only - set the eye offsets below."},
        {"vc_placement_note", "Local -Z looks forward; local +Y is up."},
        {"vc_virtual_note", "Fly to the shot, then use Snap to my view."},
        {"vc_lens_zoom_note", "Lens zoom applies to all Surface Lenses."},
        {"vc_guide_note", "Enable Show guide to draw its framing aids."},
        {"vc_look_note", "Camera look affects every face; TV effects affect one face."},
        {"vc_gate_note", "Gate program follows the Director switcher."},
        {"vc_raw_note", "Raw camera output is shown before face mapping."},
        {"vc_rate_note", "Lower picture FPS saves auxiliary scene renders."},
        {"vc_projector_hint", "Select a projector to edit its look"},
        {"vc_performance_note_1", "Adaptive protects the main view and fairly shares the capture budget."},
        {"vc_performance_note_2", "Presented FPS is measured output; the target is not a guarantee."},
        {"vc_performance_note_3", "One camera feeding many faces still renders its scene once."}
    };
    for (const auto& info : info_rows)
        if (LLUICtrl* target = control(info.first)) setValueIfChanged(target, info.second);

    mConnections.emplace_back(mModel.connectChanged(
        [this](U32 dirty) { onModelChanged(dirty); }));
    mConnections.emplace_back(
        LLDirectorCast::instance().addChangedCallback(
            refresh_bone_anchor_choices));
    mConnections.emplace_back(
        ALObjectPropertiesCache::instance().setChangeCallback(
            [this](const LLUUID& object_id)
            {
                if (!mModel.pageVisible()) return;
                for (U32 index = 0; index < mModel.registry().mProjectorCount;
                     ++index)
                    if (mModel.registry().mProjectors[index].mObjectId == object_id)
                    {
                        refresh(ALVCamPageModel::DIRTY_SELECTION);
                        break;
                    }
            }));
    const char* settings[] = {
        "PrismLensEnabled", "VCamMediaSlotsEnabled", "PrismMonitorPreviewsEnabled", "PrismMonitorInfoOverlay",
        "PrismMonitorPreviewFPS", "PrismLensZoom",
        "PrismAdaptivePerformance", "PrismProtectedMainFPS",
        "PrismCaptureRefreshCeilingHz", "PrismLensResolutionScale"
    };
    for (const char* name : settings)
        if (LLControlVariable* setting = gSavedSettings.getControl(name).get())
            mConnections.emplace_back(setting->getSignal()->connect(
                [this](LLControlVariable*, const LLSD&, const LLSD&)
                {
                    clearStatus();
                    if (mModel.pageVisible()) refresh(ALVCamPageModel::DIRTY_ALL);
                }));

    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    registrar.add("VCam.Edit", [this](LLUICtrl*, const LLSD& p) { onMonitorMenu("edit", p); });
    registrar.add("VCam.Send", [this](LLUICtrl*, const LLSD& p) { onMonitorMenu("send", p); });
    registrar.add("VCam.MoveFace", [this](LLUICtrl*, const LLSD& p) { onMonitorMenu("move", p); });
    registrar.add("VCam.Display", [this](LLUICtrl*, const LLSD& p) { onMonitorMenu("display", p); });
    registrar.add("VCam.RemoveCamera", [this](LLUICtrl*, const LLSD& p) { onMonitorMenu("remove_camera", p); });
    registrar.add("VCam.RemoveOutput", [this](LLUICtrl*, const LLSD& p) { onMonitorMenu("remove_output", p); });
    if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
            "menu_vcam_monitor.xml", gMenuHolder,
            LLViewerMenuHolderGL::child_registry_t::instance()))
        mMonitorMenu = menu->getHandle();

    return mKitPage && mMonitorHeader && mMonitorStrip && mCameraSelector &&
           mFaceList && mProjectorList;
}

void ALPanelDirectorVCam::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam page");
    if (!mStatus.empty() && mStatusTimer.getElapsedTimeF32() >= 6.f)
        clearStatus();
    LLPanel::draw();
}

void ALPanelDirectorVCam::onVisibilityChange(bool visible)
{
    mModel.setPageVisible(visible);
    LLPrismPreviewService::instance().setConsumerVisible(
        VCAM_PAGE_PREVIEW_OWNER,
        visible && gSavedSettings.getBOOL("PrismMonitorPreviewsEnabled"));
    if (!visible)
    {
        ++mMediaEditEpoch; // A picker may not commit after hiding and reopening this page.
        LLPrismPreviewService::instance().clearDemand(VCAM_PAGE_PREVIEW_OWNER);
        mPreviewDemand.fill(LLPrismLens::CaptureHandle{});
    }
    else
        refresh(ALVCamPageModel::DIRTY_ALL);
    LLPanel::onVisibilityChange(visible);
}

LLUICtrl* ALPanelDirectorVCam::control(const std::string& name) const
{
    auto found = mControls.find(name);
    return found == mControls.end() ? nullptr : found->second;
}

ALKitRow* ALPanelDirectorVCam::row(const std::string& name) const
{
    auto found = mRows.find(name);
    return found == mRows.end() ? nullptr : found->second;
}

const LLPrismLens::CaptureDefinition* ALPanelDirectorVCam::selectedCapture() const
{
    for (U32 i = 0; i < mModel.registry().mSlotCount; ++i)
        if (sameCapture(mModel.registry().mCaptures[i].mHandle,
                        mModel.selectedCapture()))
            return &mModel.registry().mCaptures[i];
    return nullptr;
}

const LLPrismLens::DisplayDefinition* ALPanelDirectorVCam::selectedDisplay() const
{
    for (U32 i = 0; i < mModel.registry().mDisplayCount; ++i)
        if (sameDisplay(mModel.registry().mDisplays[i].mHandle,
                        mModel.selectedDisplay()))
            return &mModel.registry().mDisplays[i];
    return nullptr;
}

const LLPrismLens::ProjectorDefinition* ALPanelDirectorVCam::selectedProjector() const
{
    for (U32 i = 0; i < mModel.registry().mProjectorCount; ++i)
        if (sameProjector(mModel.registry().mProjectors[i].mHandle,
                          mModel.selectedProjector()))
            return &mModel.registry().mProjectors[i];
    return nullptr;
}

void ALPanelDirectorVCam::onModelChanged(U32 dirty)
{
    if (mModel.pageVisible()) refresh(dirty);
}

void ALPanelDirectorVCam::refreshPreviewDemand()
{
    LLPrismPreviewService& preview = LLPrismPreviewService::instance();
    const bool enabled = mModel.pageVisible() &&
        gSavedSettings.getBOOL("PrismMonitorPreviewsEnabled");
    preview.setConsumerVisible(VCAM_PAGE_PREVIEW_OWNER, enabled);
    std::array<LLPrismLens::CaptureHandle, LLPrismLens::MAX_CAPTURES> desired;
    if (!enabled)
    {
        preview.clearDemand(VCAM_PAGE_PREVIEW_OWNER);
        mPreviewDemand = desired;
        return;
    }
    for (U32 i = 0; i < mModel.registry().mSlotCount; ++i)
    {
        const LLPrismLens::CaptureDefinition& capture = mModel.registry().mCaptures[i];
        desired[capture.mSlot] = capture.mHandle;
        if (!sameCapture(mPreviewDemand[capture.mSlot], capture.mHandle))
            preview.setDemand(VCAM_PAGE_PREVIEW_OWNER, capture.mHandle, true);
        preview.setMonitorSelection(capture.mSlot,
                                    mModel.monitorDisplay(capture.mSlot));
    }
    for (U32 slot = 0; slot < mPreviewDemand.size(); ++slot)
        if (mPreviewDemand[slot].mId.notNull() &&
            !sameCapture(mPreviewDemand[slot], desired[slot]))
            preview.setDemand(VCAM_PAGE_PREVIEW_OWNER, mPreviewDemand[slot], false);
    mPreviewDemand = desired;
}

void ALPanelDirectorVCam::refresh(U32 dirty)
{
    if (mRefreshing || !mModel.pageVisible()) return;
    mRefreshing = true;
    const LLPrismLens::RegistrySnapshot& registry = mModel.registry();
    if (!selectedCapture() && registry.mSlotCount)
        mModel.setSelectedCapture(registry.mCaptures[0].mHandle);
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    const LLPrismLens::ProjectorDefinition* selected_projector = selectedProjector();
    if ((!capture || !capture->isMedia()) && mMediaEditHandle.mId.notNull())
    {
        ++mMediaEditEpoch;
        mMediaEditHandle = LLPrismLens::CaptureHandle();
    }
    if (mCameraLabelRevision != registry.mConfigurationRevision)
    {
        mCameraLabels.fill(std::string());
        for (U32 index = 0; index < registry.mSlotCount; ++index)
        {
            const LLPrismLens::CaptureDefinition& item = registry.mCaptures[index];
            mCameraLabels[item.mSlot] = LLPrismLens::captureDisplayLabel(item);
        }
        mCameraLabelRevision = registry.mConfigurationRevision;
    }

    U32 visible_faces = 0;
    for (U32 i = 0; i < registry.mDisplayCount; ++i)
        if (registry.mDisplays[i].mRuntime.mVisibility ==
            LLPrismLens::EDisplayVisibility::VISIBLE)
            ++visible_faces;
    if (LLUICtrl* summary = control("vc_summary"))
    {
        const bool has_media = LLPrismLens::mediaSlotsEnabled() && registry.mSlotCount > registry.mCaptureCount;
        setValueIfChanged(summary, has_media
            ? llformat("VCam %u/8 slots · %u/16 faces (%u visible)", registry.mSlotCount, registry.mDisplayCount, visible_faces)
            : llformat("VCam %u/8 cameras · %u/16 faces (%u visible)", registry.mCaptureCount, registry.mDisplayCount, visible_faces));
    }
    // Numeric readouts are quantized by their displayed precision and share
    // a 2Hz gate even when configuration/selection forces a panel refresh.
    const F64 now = LLTimer::getTotalSeconds();
    const bool refresh_performance = now >= mNextPerformanceRefresh;
    if (refresh_performance)
    {
        mNextPerformanceRefresh = now + .5;
        const auto& perf = mModel.performance();
        if (LLUICtrl* perf_line = control("vc_perf"))
            setValueIfChanged(perf_line, !gSavedSettings.getBOOL("PrismLensEnabled")
                ? LLSD("Virtual Cam is off. Nothing renders; cameras and faces stay editable.")
                : LLSD(llformat(
                    "Presented %.0f FPS · render p95 %.1f ms · scale %.2f · admitted %.1f attempts/s · Adaptive: %s, protecting %.0f FPS",
                    perf.mPresentedFps, perf.mMainRenderP95Milliseconds,
                    perf.mUserResolutionScaleCeiling, perf.mAdmittedGlobalAttemptRateHz,
                    perf.mAdaptiveEnabled ? performanceText(perf.mState) : "off",
                    perf.mRequestedProtectedMainFps)));
        setValueIfChanged(control("vc_admitted"), llformat("%.1f of %.0f attempts/s",
            perf.mAdmittedGlobalAttemptRateHz, perf.mEffectiveTotalCaptureBudgetHz));
        if (capture)
            setValueIfChanged(control("vc_rate_readout"), llformat("%.1f / %.1f / %.1f fps - age %.2fs",
                capture->mRate.mTargetFps, capture->mRuntime.mCadenceEntitlementHz,
                capture->mRuntime.mObservedPublicationHz, capture->mRuntime.mOutputAgeSeconds));
    }
    const bool media_master = gSavedSettings.getBOOL("PrismLensEnabled");
    const bool media_slots = LLPrismLens::mediaSlotsEnabled();
    ALMediaFile::clearPlayStatusOnPolicyChange(mMediaPlayStatus,
        mMediaMasterEnabled, mMediaSlotsEnabled, media_master, media_slots);
    mMediaMasterEnabled = media_master;
    mMediaSlotsEnabled = media_slots;

    std::vector<ALVCamCameraSelector::Item> cameras;
    std::array<ALVCamMonitorStrip::Tile, LLPrismLens::MAX_CAPTURES> tiles;
    U32 monitor_slot_span = 0;
    for (U32 i = 0; i < registry.mSlotCount; ++i)
    {
        const LLPrismLens::CaptureDefinition& item = registry.mCaptures[i];
        monitor_slot_span = llmax(monitor_slot_span, item.mSlot + 1);
        cameras.push_back({item.mHandle, mCameraLabels[item.mSlot],
                           item.mRuntime.mActivity == LLPrismLens::EActivityState::LIVE});
        ALVCamMonitorStrip::Tile& tile = tiles[item.mSlot];
        tile.mCapture = item.mHandle;
        tile.mPublication = LLPrismPreviewService::instance().tileState(item.mHandle);
        tile.mLabel = item.mLabel;
        tile.mMedia = item.isMedia();
        tile.mActivity = item.mRuntime.mActivity;
        tile.mObservedFps = item.mRuntime.mObservedPublicationHz;
        tile.mTargetFps = item.mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS
            ? item.mRate.mTargetFps : item.mRuntime.mCadenceEntitlementHz;
        tile.mPreviewFps = gSavedSettings.getF32("PrismMonitorPreviewFPS");
        tile.mFaceCount = item.mDisplayCount;
        tile.mEditing = sameCapture(item.mHandle, mModel.selectedCapture());
        tile.mMasterEnabled = gSavedSettings.getBOOL("PrismLensEnabled");
        tile.mPreviewsEnabled = gSavedSettings.getBOOL("PrismMonitorPreviewsEnabled");
        tile.mNoSignal = item.isMedia() && !item.mRuntime.mHasOutput;
        for (U32 pi = 0; pi < registry.mProjectorCount; ++pi)
            if (sameCapture(registry.mProjectors[pi].mCapture, item.mHandle))
            {
                tile.mNoSignalWhite = registry.mProjectors[pi].mSettings.mNoSignal == LLPrismLens::ENoSignal::WHITE;
                break;
            }
        if (!tile.mMasterEnabled) tile.mStatus = "Error: Turn on Virtual Cam";
        else if (!tile.mPreviewsEnabled) tile.mStatus = "Disabled";
        else
        {
            const auto edit = mMediaRefusals.find(item.mHandle.mId);
            const bool current = edit != mMediaRefusals.end() && edit->second.generation == item.mHandle.mGeneration &&
                item.mediaUrlData() && edit->second.cue == item.mediaUrlData()->mValue;
            const auto error = ALMediaFile::mediaSourceStatus(item.mRuntime.mReason,
                current ? edit->second.reason : "", item.mRuntime.mReasonSequence, current ? edit->second.sequence : 0);
            tile.mStatus = !error.empty() ? "Error: " + error :
                !item.mRuntime.mHasOutput ? "Error: No source" : "Preview";
        }

        const LLPrismLens::DisplayHandle monitor = mModel.monitorDisplay(item.mSlot);
        if (monitor.mId.notNull())
            for (U32 di = 0; di < registry.mDisplayCount; ++di)
                if (sameDisplay(registry.mDisplays[di].mHandle, monitor))
                {
                    tile.mMonitorShowsDisplay = true;
                    tile.mMonitorDisplayVirtual = registry.mDisplays[di].mSettings.mVirtual;
                    tile.mMonitorDisplayIndex = di;
                    break;
                }
        tile.mVisible = true;
    }
    mCameraSelector->setItems(cameras, mModel.selectedCapture());
    mCameraSelector->setActionsEnabled(registry.mSlotCount < 8, capture != nullptr, LLPrismLens::mediaSlotsEnabled());
    const bool info_overlay = gSavedSettings.getBOOL("PrismMonitorInfoOverlay");
    mMonitorStrip->setTiles(tiles, monitor_slot_span, mCompact, info_overlay, mLargePage);
    mMonitorStrip->setNewFaceFit(mNewFaceFit);
    if (mMonitorPager)
    {
        mMonitorPager->setPage(mLargePage, monitor_slot_span);
        setRowControlVisible(row("vc_monitor_pager"), !mCompact && monitor_slot_span > 4);
    }

    std::vector<ALVCamFaceList::Item> faces;
    if (capture)
    {
        faces.push_back({{}, capture->isMedia() ? "Media output (raw)" : "Camera output (raw)", "before any face mapping", true,
                         mModel.selectedDisplay().mId.isNull()});
        for (U32 i = 0; i < registry.mDisplayCount; ++i)
        {
            const LLPrismLens::DisplayDefinition& face = registry.mDisplays[i];
            if (face.mGateSubscribed || !sameCapture(face.mCapture, capture->mHandle)) continue;
            faces.push_back({face.mHandle, displayName(face, i),
                face.mSettings.mVirtual ? "Virtual screen" : "Prim face",
                false, sameDisplay(mModel.selectedDisplay(), face.mHandle)});
        }
        // Gate-subscribed faces follow the Director switcher, so they belong to
        // no single camera: list them under every Camera Feed, after its own
        // faces. A Surface Lens owns exactly one face and cannot take them.
        for (U32 i = 0; i < registry.mDisplayCount; ++i)
        {
            const LLPrismLens::DisplayDefinition& face = registry.mDisplays[i];
            if (!face.mGateSubscribed ||
                !capture->isCameraFeed()) continue;
            faces.push_back({face.mHandle, faceListName(face, i),
                "Gate program", false,
                sameDisplay(mModel.selectedDisplay(), face.mHandle)});
        }
    }
    mFaceList->setItems(faces);
    mFaceList->setMovePick(mMoveTargetCapture.mId.notNull());
    if (dirty & (ALVCamPageModel::DIRTY_CONFIGURATION |
                 ALVCamPageModel::DIRTY_SELECTION))
        mProjectorNames.clear();
    std::vector<ALVCamProjectorList::Item> projectors;
    std::string selected_projector_name;
    if (capture)
        for (U32 index = 0; index < registry.mProjectorCount; ++index)
        {
            const LLPrismLens::ProjectorDefinition& projector = registry.mProjectors[index];
            if (!sameCapture(projector.mCapture, capture->mHandle)) continue;
            // Cache only final names (rig role or the object's real name); a
            // fallback ("Light (waiting)", "Light at x,y,z") is re-resolved
            // on later refreshes so the real name appears once it's known.
            auto cached_name = mProjectorNames.find(projector.mHandle.mId);
            std::string fallback_name;
            if (cached_name == mProjectorNames.end())
            {
                std::string resolved = projectorName(projector);
                if (resolved.rfind("Light at ", 0) == 0 || resolved == "Light (waiting)")
                    fallback_name = std::move(resolved);
                else
                    cached_name = mProjectorNames.emplace(
                        projector.mHandle.mId, std::move(resolved)).first;
            }
            const std::string& name = fallback_name.empty()
                ? cached_name->second : fallback_name;
            const bool selected = sameProjector(projector.mHandle,
                                                mModel.selectedProjector());
            projectors.push_back({projector.mHandle, name,
                projectorState(projector),
                selected});
            if (selected) selected_projector_name = name;
        }
    mProjectorList->setItems(projectors);
    if (ALKitCard* projector_card = mKitPage->findCard("projectors"))
        projector_card->setCodeVisible(!projectors.empty());
    setRowControlVisible(row("vc_projector_hint"),
                         !projectors.empty() && selected_projector == nullptr);
    const std::string projector_badge = selected_projector_name.empty()
        ? "PROJECTOR" : selected_projector_name;
    for (const char* id : { "projector", "projector_tv" })
        if (ALKitCard* projector_card = mKitPage->findCard(id))
        {
            projector_card->setBadgePlaceholder("PROJECTOR n", projector_badge);
            projector_card->setCodeVisible(selected_projector != nullptr);
        }
    updateCardState(capture != nullptr, display != nullptr,
                    capture ? mCameraLabels[capture->mSlot]
                            : std::string());

    if (selected_projector)
    {
        const LLPrismLens::ProjectorSettings& settings = selected_projector->mSettings;
        const LLPrismLens::ProjectorLook& look = settings.mLook;
        const LLPrismLens::ScreenEffects& effect = settings.mEffects;
        setValueIfChanged(projectorControl(PROJ_LOOK), static_cast<S32>(
            ALVCamPageModel::projectorLookPreset(look)));
        setValueIfChanged(projectorControl(PROJ_FIT), static_cast<S32>(settings.mFitMode));
        setValueIfChanged(projectorControl(PROJ_ANCHOR_X), settings.mAnchor[0]);
        setValueIfChanged(projectorControl(PROJ_ANCHOR_Y), settings.mAnchor[1]);
        setValueIfChanged(projectorControl(PROJ_FLIP_H), settings.mFlipH);
        setValueIfChanged(projectorControl(PROJ_FLIP_V), settings.mFlipV);
        setValueIfChanged(projectorControl(PROJ_ROTATE), static_cast<S32>(settings.mRotate));
        setValueIfChanged(projectorControl(PROJ_KEYSTONE_H), look.mKeystoneH);
        setValueIfChanged(projectorControl(PROJ_KEYSTONE_V), look.mKeystoneV);
        setValueIfChanged(projectorControl(PROJ_THROW), static_cast<S32>(look.mThrowMode));
        setEnabledIfChanged(projectorControl(PROJ_THROW), capture && capture->isCameraFeed());
        setEnabledIfChanged(projectorControl(PROJ_FOLLOW_EXPOSURE), capture && capture->isCameraFeed());
        setValueIfChanged(projectorControl(PROJ_BRIGHTNESS), settings.mFeedEV);
        setValueIfChanged(projectorControl(PROJ_LAMP_POWER), settings.mLampPowerEV);
        setValueIfChanged(projectorControl(PROJ_FOLLOW_EXPOSURE), settings.mFollowMainExposure);
        setValueIfChanged(projectorControl(PROJ_LAMP_KELVIN), look.mLampKelvin);
        setValueIfChanged(projectorControl(PROJ_GEL_TINT),
            LLColor4(look.mGelTint, 1.f).getValue());
        setValueIfChanged(projectorControl(PROJ_HOTSPOT), look.mHotspot);
        setValueIfChanged(projectorControl(PROJ_SHUTTER_FLICKER), look.mShutterFlicker);
        setValueIfChanged(projectorControl(PROJ_SHUTTER_RATE), look.mShutterHz);
        setValueIfChanged(projectorControl(PROJ_GATE_WEAVE), look.mGateWeave);
        setValueIfChanged(projectorControl(PROJ_GRAIN), look.mGrain);
        setValueIfChanged(projectorControl(PROJ_DUST), look.mDust);
        setValueIfChanged(projectorControl(PROJ_SCRATCHES), look.mScratches);
        setValueIfChanged(projectorControl(PROJ_BURN), look.mBurn);
        setValueIfChanged(projectorControl(PROJ_HALATION), look.mHalation);
        setValueIfChanged(projectorControl(PROJ_RESOLUTION), static_cast<S32>(settings.mResolution));
        setValueIfChanged(projectorControl(PROJ_NO_SIGNAL), static_cast<S32>(settings.mNoSignal));
        setValueIfChanged(projectorControl(PROJ_KEEP_GOBO), settings.mKeepGobo);
        setValueIfChanged(projectorControl(PROJ_TV_PRESET), LLSD());
        setValueIfChanged(projectorControl(PROJ_TV_SCAN), effect.mScanlines);
        setValueIfChanged(projectorControl(PROJ_TV_GRAY), effect.mGrayscale);
        setValueIfChanged(projectorControl(PROJ_TV_STATIC), effect.mStatic);
        setValueIfChanged(projectorControl(PROJ_TV_VIGNETTE), effect.mVignette);
        setValueIfChanged(projectorControl(PROJ_TV_BRIGHTNESS), effect.mBrightness);
        setValueIfChanged(projectorControl(PROJ_TV_LINES), 100.f + 500.f * effect.mScanlineCount);
        setValueIfChanged(projectorControl(PROJ_TV_DARK), effect.mScanlineDepth * 100.f);
        setValueIfChanged(projectorControl(PROJ_TV_SHARP), effect.mScanlineSharpness * 100.f);
        setValueIfChanged(projectorControl(PROJ_TV_THICK), effect.mScanlineThickness * 100.f);
        setValueIfChanged(projectorControl(PROJ_TV_PIXEL), effect.mPixelate);
        setValueIfChanged(projectorControl(PROJ_TV_SEPIA), effect.mSepia);
        setValueIfChanged(projectorControl(PROJ_TV_CHROMA), effect.mChromaBleed);
        setValueIfChanged(projectorControl(PROJ_TV_VROLL), effect.mVerticalRoll);
        setValueIfChanged(projectorControl(PROJ_TV_ROLL_SPEED), effect.mRollSpeed);
        setValueIfChanged(projectorControl(PROJ_TV_TRACKING), effect.mTracking);
        setValueIfChanged(projectorControl(PROJ_TV_FLICKER), effect.mFlicker);
        setValueIfChanged(projectorControl(PROJ_TV_INTERLACE), effect.mInterlace);
        setValueIfChanged(projectorControl(PROJ_TV_DROPOUT), effect.mDropout);
        setValueIfChanged(projectorControl(PROJ_TV_SHEEN), effect.mSheen);
    }

    // A Gate face belongs to no single camera, so it can always be removed;
    // other faces only under an ordinary Camera Feed.
    setEnabledIfChanged(control("vc_remove_face"), display &&
        (display->mGateSubscribed ||
         (capture && !capture->isSurfaceLens())));
    setEnabledIfChanged(mCameraName, capture != nullptr);
    if (mCameraName && !gFocusMgr.childHasKeyboardFocus(mCameraName))
        setValueIfChanged(mCameraName,
                          capture ? LLSD(capture->mLabel) : LLSD());
    setEnabledIfChanged(control("vc_face_locate"), display && !display->mSettings.mVirtual);
    if (LLUICtrl* count = control("vc_face_count"))
        setValueIfChanged(count, llformat("%u of 16 faces used (all cameras)",
                                         registry.mDisplayCount));

    if (capture && capture->cameraData())
    {
        const LLPrismLens::CameraSettings& camera = capture->cameraData()->mSettings;
        const LLPrismLens::BonePovSettings& bone = camera.mBonePov;
        const bool surface = capture->isSurfaceLens();
        const bool test_pattern = !surface && camera.mTestPattern;
        const bool skeleton = !surface && !test_pattern && bone.mEnabled;
        const bool virtual_camera = !surface && !test_pattern && camera.mVirtual && !skeleton;
        const bool object_camera = !surface && !test_pattern && !camera.mVirtual;
        const bool fixed_fov = camera.mFovMode == LLPrismLens::EFovMode::FIXED;
        if (ALKitCard* skeleton_card = mKitPage->findCard("skeleton"))
            skeleton_card->setCodeVisible(!surface && !test_pattern && camera.mVirtual);
        static const char* const CAMERA_CONTROL_CARDS[] = {
            "placement", "optics", "guide", "camera_look", "output_frame"
        };
        for (const char* card_id : CAMERA_CONTROL_CARDS)
            if (ALKitCard* card = mKitPage->findCard(card_id))
                card->setCodeVisible(!test_pattern);
        setValueIfChanged(control("vc_source_bound"), surface ? "Surface aperture" :
            test_pattern ? "Local SMPTE orientation pattern" :
            skeleton ? ("Driven by " + bone.mCustomJoint) :
            virtual_camera ? llformat("Stored at %.1f, %.1f, %.1f", camera.mVirtualPos.mV[0],
                                      camera.mVirtualPos.mV[1], camera.mVirtualPos.mV[2]) :
            capture->cameraData()->mObjectId.asString());
        setValueIfChanged(control("vc_source_type"), test_pattern ? 3 :
            skeleton ? 2 : virtual_camera ? 1 : 0);
        setEnabledIfChanged(control("vc_source_type"), !surface);
        if (surface)
        {
            const LLPrismLens::DisplayDefinition* aperture = nullptr;
            for (U32 i = 0; i < registry.mDisplayCount; ++i)
                if (sameCapture(registry.mDisplays[i].mCapture, capture->mHandle))
                {
                    aperture = &registry.mDisplays[i];
                    break;
                }
            setValueIfChanged(control("vc_lens_aperture"), aperture
                ? LLSD(llformat("Object %s · face %d",
                    aperture->mDisplayObjectId.asString().c_str(),
                    aperture->mDisplayTextureEntry))
                : LLSD("Aperture unavailable"));
        }
        setRowControlVisible(row("vc_source_note_object"), object_camera);
        setRowControlVisible(row("vc_source_note_virtual"), virtual_camera);
        setRowControlVisible(row("vc_source_note_skeleton"), skeleton);
        setRowControlVisible(row("vc_set_selected_camera"), object_camera);
        setRowControlVisible(row("vc_place_eye_front"), object_camera);
        setRowControlVisible(row("vc_placement_note"), object_camera);
        setRowControlVisible(row("vc_eye_x"), object_camera);
        setRowControlVisible(row("vc_eye_y"), object_camera);
        setRowControlVisible(row("vc_eye_z"), object_camera);
        setRowControlVisible(row("vc_snap_view"), virtual_camera);
        setRowControlVisible(row("vc_virtual_note"), virtual_camera);
        setRowControlVisible(row("vc_lens_type"), surface);
        setRowControlVisible(row("vc_lens_aperture"), surface);
        setRowControlVisible(row("vc_lens_locate"), surface);
        setRowControlVisible(row("vc_lens_note"), surface);
        setRowControlVisible(row("vc_fov_mode"), !skeleton);
        setRowControlVisible(row("vc_fov_deg"), !skeleton && fixed_fov);
        setRowControlVisible(row("vc_fov_projector"), !skeleton && !fixed_fov);
        setRowControlVisible(row("vc_fov_attached"), skeleton);
        setRowControlVisible(row("vc_guide_thirds"), camera.mShowGuide);
        setRowControlVisible(row("vc_guide_up"), camera.mShowGuide);
        setRowControlVisible(row("vc_guide_cross"), camera.mShowGuide);
        setRowControlVisible(row("vc_guide_clip"), camera.mShowGuide);
        setRowControlVisible(row("vc_guide_note"), !camera.mShowGuide);
        setRowControlVisible(row("vc_output_custom"), mCustomAspect);

        setValueIfChanged(control("vc_eye_x"), camera.mLocalEyeOffset.mV[0]);
        setValueIfChanged(control("vc_eye_y"), camera.mLocalEyeOffset.mV[1]);
        setValueIfChanged(control("vc_eye_z"), camera.mLocalEyeOffset.mV[2]);
        setValueIfChanged(control("vc_fov_mode"), static_cast<S32>(camera.mFovMode));
        setValueIfChanged(control("vc_fov_deg"), camera.mFixedVerticalFovRad * RAD_TO_DEG);
        setValueIfChanged(control("vc_fov_projector"), llformat("From projector (%.1f°)",
            capture->mRuntime.mEffectiveVerticalFovRad * RAD_TO_DEG));
        setValueIfChanged(control("vc_fov_attached"), bone.mFovDeg);
        setValueIfChanged(control("vc_near_clip"), camera.mNearClip);
        setValueIfChanged(control("vc_far_clip"), camera.mFarClip);
        setValueIfChanged(control("vc_guide_show"), camera.mShowGuide);
        setValueIfChanged(control("vc_guide_thirds"), camera.mGuideThirds);
        setValueIfChanged(control("vc_guide_up"), camera.mGuideUpRoll);
        setValueIfChanged(control("vc_guide_cross"), camera.mGuideCrosshair);
        setValueIfChanged(control("vc_guide_clip"), camera.mGuideClipMarkers);
        setValueIfChanged(control("vc_exposure"), camera.mOptics.mExposureBias);
        setValueIfChanged(control("vc_grain"), camera.mOptics.mFilmGrain);
        setValueIfChanged(control("vc_chromatic"), camera.mOptics.mChromaticAberration);
        setValueIfChanged(control("vc_camera_crt"), camera.mOptics.mCRTScanlines);
        setAspectValue("vc_output_aspect", camera.mOutputAspect);
        setValueIfChanged(control("vc_output_custom"), camera.mOutputAspect);
        const F32 fov = skeleton ? bone.mFovDeg : camera.mFixedVerticalFovRad * RAD_TO_DEG;
        setValueIfChanged(control("vc_lens_equiv"), llformat("%.0f mm full frame - %.1f deg V",
            12.f / std::tan(llmax(1.f, fov) * DEG_TO_RAD * .5f), fov));
        setValueIfChanged(control("vc_output_picture"), llformat("1024 x %d px (scale %.2f)",
            ll_round(1024.f / llmax(.25f, camera.mOutputAspect)),
            mModel.performance().mUserResolutionScaleCeiling));
        setValueIfChanged(control("vc_look_feeds"), llformat("%u bound face%s",
            capture->mDisplayCount, capture->mDisplayCount == 1 ? "" : "s"));

        ALDirectorCastUI::showStoredValue(
            dynamic_cast<LLComboBox*>(control("vc_bone_anchor")),
            LLSD(bone.mAnchorSlot), 0, 1);
        setValueIfChanged(control("vc_bone_enabled"), bone.mEnabled);
        setValueIfChanged(control("vc_bone_joint"), bone.mJointSelection == LLPrismLens::BONE_JOINT_EYELINE
            ? LLSD("") : LLSD(bone.mCustomJoint));
        setValueIfChanged(control("vc_bone_aim"), bone.mAimMode);
        setValueIfChanged(control("vc_bone_roll"), bone.mRollMode);
        setValueIfChanged(control("vc_bone_smooth"), bone.mSmoothingSec);
        setValueIfChanged(control("vc_bone_x"), bone.mOffset.mV[0]);
        setValueIfChanged(control("vc_bone_y"), bone.mOffset.mV[1]);
        setValueIfChanged(control("vc_bone_z"), bone.mOffset.mV[2]);
        setValueIfChanged(control("vc_bone_pitch"), bone.mTrimPitchDeg);
        setValueIfChanged(control("vc_bone_yaw"), bone.mTrimYawDeg);
        setValueIfChanged(control("vc_bone_scale"), bone.mScaleAware);
        setValueIfChanged(control("vc_bone_status"), ALVCamBonePov::instance().status(capture->mHandle));

    }

    if (display)
    {
        const LLPrismLens::DisplaySettings& settings = display->mSettings;
        const LLPrismLens::ScreenEffects& effect = settings.mEffects;
        setValueIfChanged(control("vc_face_feed"), display->mGateSubscribed ? 1 : 0);
        setValueIfChanged(control("vc_face_fit"), static_cast<S32>(settings.mFitMode));
        const char* visibility = display->mRuntime.mVisibility ==
            LLPrismLens::EDisplayVisibility::VISIBLE ? "visible" : "offscreen";
        setValueIfChanged(control("vc_face_shape"), settings.mVirtual
            ? llformat("%.2f x %.2f m virtual screen · %s", settings.mWidth,
                       settings.mHeight, visibility)
            : llformat("Prim face %d · %s", display->mDisplayTextureEntry, visibility));
        setRowControlVisible(row("vc_anchor_x"), settings.mFitMode == LLPrismLens::EFitMode::FILL);
        setRowControlVisible(row("vc_anchor_y"), settings.mFitMode == LLPrismLens::EFitMode::FILL);
        setRowControlVisible(row("vc_screen_reposition"), settings.mVirtual);
        setRowControlVisible(row("vc_screen_height"), settings.mVirtual);
        setRowControlVisible(row("vc_screen_aspect"), settings.mVirtual);
        setRowControlVisible(row("vc_gate_note"), display->mGateSubscribed);
        setRowControlVisible(row("vc_raw_note"), false);
        setValueIfChanged(control("vc_anchor_x"), settings.mAnchor[0]);
        setValueIfChanged(control("vc_anchor_y"), settings.mAnchor[1]);
        setValueIfChanged(control("vc_screen_height"), settings.mHeight);
        setAspectValue("vc_screen_aspect", settings.mWidth / llmax(.01f, settings.mHeight));
        setValueIfChanged(control("vc_bar_r"), settings.mBarColorLinear[0]);
        setValueIfChanged(control("vc_bar_g"), settings.mBarColorLinear[1]);
        setValueIfChanged(control("vc_bar_b"), settings.mBarColorLinear[2]);
        setValueIfChanged(control("vc_tv_scan"), effect.mScanlines);
        setValueIfChanged(control("vc_tv_gray"), effect.mGrayscale);
        setValueIfChanged(control("vc_tv_static"), effect.mStatic);
        setValueIfChanged(control("vc_tv_vignette"), effect.mVignette);
        setValueIfChanged(control("vc_tv_brightness"), effect.mBrightness);
        setValueIfChanged(control("vc_tv_flip_h"), effect.mFlipH);
        setValueIfChanged(control("vc_tv_flip_v"), effect.mFlipV);
        setValueIfChanged(control("vc_tv_rotate"), effect.mRotate90);
        setValueIfChanged(control("vc_tv_lines"), 100.f + 500.f * effect.mScanlineCount);
        setValueIfChanged(control("vc_tv_dark"), effect.mScanlineDepth * 100.f);
        setValueIfChanged(control("vc_tv_sharp"), effect.mScanlineSharpness * 100.f);
        setValueIfChanged(control("vc_tv_thick"), effect.mScanlineThickness * 100.f);
        setValueIfChanged(control("vc_tv_pixel"), effect.mPixelate);
        setValueIfChanged(control("vc_tv_sepia"), effect.mSepia);
        setValueIfChanged(control("vc_tv_chroma"), effect.mChromaBleed);
        setValueIfChanged(control("vc_tv_vroll"), effect.mVerticalRoll);
        setValueIfChanged(control("vc_tv_roll_speed"), effect.mRollSpeed);
        setValueIfChanged(control("vc_tv_tracking"), effect.mTracking);
        setValueIfChanged(control("vc_tv_flicker"), effect.mFlicker);
        setValueIfChanged(control("vc_tv_interlace"), effect.mInterlace);
        setValueIfChanged(control("vc_tv_dropout"), effect.mDropout);
        setValueIfChanged(control("vc_tv_sheen"), effect.mSheen);
    }
    else
    {
        setRowControlVisible(row("vc_anchor_x"), false);
        setRowControlVisible(row("vc_anchor_y"), false);
        setRowControlVisible(row("vc_screen_reposition"), false);
        setRowControlVisible(row("vc_screen_height"), false);
        setRowControlVisible(row("vc_screen_aspect"), false);
        setRowControlVisible(row("vc_gate_note"), false);
        setRowControlVisible(row("vc_raw_note"), capture != nullptr);
    }

    const bool media_selected = capture && capture->isMedia();
    if (ALKitCard* lens_card = mKitPage->findCard("surface_lens"))
        lens_card->setCodeVisible(!media_selected);
    if (ALKitCard* media_card = mKitPage->findCard("media_source"))
    {
        media_card->setCodeVisible(media_selected && LLPrismLens::mediaSlotsEnabled());
        if (media_selected) media_card->setBadgePlaceholder("CAM n", llformat("Cam %u", capture->mSlot + 1));
    }
    if (media_selected)
    {
        for (const char* id : {"source", "placement", "skeleton", "optics", "guide", "camera_look", "look_goes", "output_frame"})
            if (ALKitCard* card = mKitPage->findCard(id)) card->setCodeVisible(false);
        const auto* video = capture->mediaUrlData();
        const bool enabled = LLPrismLens::mediaSlotsEnabled();
        setValueIfChanged(control("vc_media_kind"), video ? "url" : "face");
        for (const char* id : {"vc_media_video", "vc_media_browse", "vc_media_transport", "vc_media_loop", "vc_media_volume", "vc_media_cache_clear", "vc_media_cache_status", "vc_media_cache_keep"})
            setRowControlVisible(row(id), video != nullptr);
        for (const char* id : {"vc_media_face", "vc_media_face_actions"})
            setRowControlVisible(row(id), video == nullptr);
        if (!sameCapture(mMediaEditHandle, capture->mHandle) || mMediaEditingUrl != (video != nullptr) ||
            (video && video->mValue != mMediaCue))
        {
            ++mMediaEditEpoch;
            if (!sameCapture(mMediaEditHandle, capture->mHandle) || mMediaEditingUrl != (video != nullptr))
                mMediaPlayStatus.clear();
            mMediaEditHandle = capture->mHandle;
            mMediaEditingUrl = video != nullptr;
            mMediaCue = video ? video->mValue : "";
            mMediaDraft = mMediaCue;
            mMediaError.clear();
            mMediaValidationSequence = 0;
            mMediaPlayable = !mMediaCue.empty();
            const auto refused = mMediaRefusals.find(capture->mHandle.mId);
            if (video && refused != mMediaRefusals.end() &&
                refused->second.generation == capture->mHandle.mGeneration && refused->second.cue == mMediaCue)
            {
                mMediaDraft = refused->second.draft;
                mMediaError = mMediaPlayStatus = refused->second.reason;
                mMediaValidationSequence = refused->second.sequence;
                mMediaPlayable = mMediaError.empty() && !mMediaDraft.empty();
            }
            setValueIfChanged(mMediaVideo, mMediaDraft);
        }
        if (video)
        {
            setValueIfChanged(control("vc_media_loop"), video->mLoop);
            setValueIfChanged(control("vc_media_volume"), video->mVolume);
        }
        setEnabledIfChanged(control("vc_media_play"), enabled && video && mMediaPlayable);
        setEnabledIfChanged(control("vc_media_pause"), enabled && video && capture->mMediaCanPause);
        setEnabledIfChanged(control("vc_media_stop"), enabled && video);
        setEnabledIfChanged(control("vc_media_running"), enabled &&
            (!capture->mMediaRunning || capture->mRuntime.mHasOutput));
        if (auto* freeze = dynamic_cast<LLButton*>(control("vc_media_running")))
        {
            const std::string label = capture->mMediaRunning ? "Freeze" : "Resume";
            setLabelIfChanged(freeze, label);
        }
        setValueIfChanged(control("vc_media_running"), capture->mMediaRunning);
        setValueIfChanged(control("vc_media_status"), !gSavedSettings.getBOOL("PrismLensEnabled") ? "Turn on Virtual Cam to play video" : ALMediaFile::mediaSourceStatus(capture->mRuntime.mReason,
            !mMediaPlayStatus.empty() ? mMediaPlayStatus : !mMediaError.empty() ? mMediaError :
            video && mMediaDraft.empty() ? "Enter a direct video URL" :
            !video && capture->mediaData()->mObjectId.isNull() ? "Pick a prim face" :
            mMediaValidationSequence > capture->mRuntime.mReasonSequence ? "Set. Press Play to start it." : capture->mRuntime.mReason,
            capture->mRuntime.mReasonSequence, mMediaValidationSequence));
        if (mMediaVideo) mMediaVideo->setFgColor(ALKitStyle::color(
            !mMediaError.empty() || !mMediaPlayStatus.empty() ? ALKitStyle::RECORD : ALKitStyle::TEXT));
        setValueIfChanged(control("vc_media_note"), video
            ? "Only you see and hear this. Volume starts muted. Local files work only on this PC; scenes never auto-play."
            : "Only you see these outputs; audio stays on the prim.");
        refreshMediaCache();
        setValueIfChanged(control("vc_source_aspect"), static_cast<S32>(capture->mediaAspect()));
        std::string face_name = "None picked";
        if (const auto* face = capture->mediaData(); face && face->mObjectId.notNull())
        {
            const auto* properties = ALObjectPropertiesCache::instance().get(face->mObjectId);
            face_name = llformat("%s / face %d", properties && !properties->mName.empty()
                ? properties->mName.c_str() : face->mObjectId.asString().c_str(), face->mTE);
        }
        setValueIfChanged(control("vc_media_face"), face_name);

    }

    if (capture)
    {
        setValueIfChanged(control("vc_rate_mode"), static_cast<S32>(capture->mRate.mMode));
        setValueIfChanged(control("vc_target_fps"), capture->mRate.mTargetFps);
        setRowControlVisible(row("vc_target_fps"), capture->mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS);
        setRowControlVisible(row("vc_rate_preset"), capture->mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS);
    }

    mMonitorHeader->setState(mCompact,
        gSavedSettings.getBOOL("PrismMonitorPreviewsEnabled"),
        info_overlay,
        gSavedSettings.getF32("PrismMonitorPreviewFPS"));
    setValueIfChanged(control("vc_lens_zoom"), gSavedSettings.getF32("PrismLensZoom"));
    setValueIfChanged(control("vc_protect"), gSavedSettings.getBOOL("PrismAdaptivePerformance"));
    setValueIfChanged(control("vc_main_fps"), gSavedSettings.getF32("PrismProtectedMainFPS"));
    setValueIfChanged(control("vc_capture_budget"), gSavedSettings.getF32("PrismCaptureRefreshCeilingHz"));
    setValueIfChanged(control("vc_quality_ceiling"), gSavedSettings.getF32("PrismLensResolutionScale"));
    setValueIfChanged(control("vc_preview_cost"), mMonitorStrip->previewCostText(
        gSavedSettings.getBOOL("PrismMonitorPreviewsEnabled"),
        gSavedSettings.getF32("PrismMonitorPreviewFPS")));
    U32 preview_only = 0;
    for (U32 ci = 0; ci < registry.mSlotCount; ++ci)
    {
        bool has_visible_face = false;
        for (U32 di = 0; di < registry.mDisplayCount; ++di)
            if (sameCapture(registry.mDisplays[di].mCapture, registry.mCaptures[ci].mHandle) &&
                registry.mDisplays[di].mRuntime.mVisibility == LLPrismLens::EDisplayVisibility::VISIBLE)
            {
                has_visible_face = true;
                break;
            }
        if (!has_visible_face) ++preview_only;
    }
    const bool master_enabled = gSavedSettings.getBOOL("PrismLensEnabled");
    const bool previews_enabled = gSavedSettings.getBOOL("PrismMonitorPreviewsEnabled");
    setValueIfChanged(control("vc_monitor_footer"), mMonitorStrip->monitorFooterText(
        master_enabled, previews_enabled, registry.mSlotCount, preview_only,
        gSavedSettings.getF32("PrismMonitorPreviewFPS")));

    if (dirty & (ALVCamPageModel::DIRTY_CONFIGURATION |
                 ALVCamPageModel::DIRTY_SELECTION))
        refreshPreviewDemand();
    mRefreshing = false;
}

void ALPanelDirectorVCam::refreshMediaCache()
{
    const auto cache = ALMediaFile::cacheStatus(); // Snapshot only; never initializes I/O.
    setEnabledIfChanged(control("vc_media_cache_clear"), LLPrismLens::mediaSlotsEnabled() && cache.owner);
    setValueIfChanged(control("vc_media_cache_status"), !gSavedSettings.getBOOL("PrismLensEnabled") ? "Turn on Virtual Cam to play video" : cache.owner
        ? llformat("%.1f / %.1f MiB; %s", cache.bytes / 1048576.0, cache.limit / 1048576.0, cache.status.c_str())
        : !cache.status.empty() ? cache.status : cache.checking ? "Checking video cache..." : "Video cache idle");
}

void ALPanelDirectorVCam::validateMediaVideo()
{
    if (!mMediaVideo) return;
    mMediaDraft = mMediaVideo->getText();
    ALMediaFile::Descriptor syntax;
    mMediaError.clear();
    mMediaPlayStatus.clear();
    mMediaPlayable = ALMediaFile::classify(mMediaDraft, syntax, mMediaError, LL_WINDOWS) && !syntax.value.empty();
    setEnabledIfChanged(control("vc_media_play"), LLPrismLens::mediaSlotsEnabled() &&
        mMediaPlayable);
    const auto* capture = selectedCapture();
    mMediaValidationSequence = ALMediaFile::nextMediaStatusSequence();
    // Keystrokes validate only the Source editor. Monitor refusals are recorded
    // by commitMediaVideo/refuseMediaVideo after Enter, focus loss, paste/Browse.
    mModel.markDirty(ALVCamPageModel::DIRTY_RUNTIME);
    setValueIfChanged(control("vc_media_status"), !gSavedSettings.getBOOL("PrismLensEnabled") ? "Turn on Virtual Cam to play video" : ALMediaFile::mediaSourceStatus(capture ? capture->mRuntime.mReason : "",
        !mMediaPlayStatus.empty() ? mMediaPlayStatus : !mMediaError.empty() ? mMediaError :
        mMediaDraft.empty() ? "Enter a direct video URL" : "Set. Press Play to start it.",
        capture ? capture->mRuntime.mReasonSequence : 0, mMediaValidationSequence));
}

void ALPanelDirectorVCam::refuseMediaVideo(const std::string& reason)
{
    mMediaError = mMediaPlayStatus = reason;
    mMediaPlayable = false;
    mMediaValidationSequence = ALMediaFile::nextMediaStatusSequence();
    mMediaRefusals[mMediaEditHandle.mId] = {mMediaEditHandle.mGeneration, mMediaCue, mMediaDraft, reason, mMediaValidationSequence};
    mModel.markDirty(ALVCamPageModel::DIRTY_RUNTIME);
}

bool ALPanelDirectorVCam::commitMediaVideo()
{
    const auto* capture = selectedCapture();
    if (!capture || !capture->mediaUrlData() || !sameCapture(capture->mHandle, mMediaEditHandle))
    {
        LL_INFOS("VCamMedia") << "Typed commit invalid rule=selected private slot/generation" << LL_ENDL;
        return false;
    }
    validateMediaVideo();
    const bool valid = ALMediaFile::commitVideoDraft(mMediaDraft, mMediaError, LL_WINDOWS,
        [&](const ALMediaFile::Descriptor& syntax)
    {
        auto source = *capture->mediaUrlData();
        source.mValue = mMediaDraft; // Keep exactly what the user typed.
        source.mLocation = syntax.location == ALMediaFile::Location::LOCAL_FILE
            ? LLPrismLens::MediaUrlSource::Location::LOCAL_FILE : LLPrismLens::MediaUrlSource::Location::HTTP_URL;
        return LLPrismLens::setMediaUrlSource(capture->mHandle, source, &mMediaError);
    });
    LL_INFOS("VCamMedia") << "Typed commit " << (valid ? "valid" : "invalid")
        << " slot=" << capture->mSlot << " rule=syntax/descriptor reason=" << (valid ? "accepted cue" : mMediaError)
        << " ext=" << ALMediaFile::diagnosticExtension(mMediaDraft) << LL_ENDL;
    if (!valid) refuseMediaVideo(mMediaError);
    else mMediaRefusals[capture->mHandle.mId] = {capture->mHandle.mGeneration, mMediaDraft, mMediaDraft, "", mMediaValidationSequence};
    return valid;
}

void ALPanelDirectorVCam::setStatus(const std::string& text, bool warning)
{
    mStatus = text;
    mStatusWarning = warning;
    mStatusTimer.reset();
    if (LLUICtrl* status = control("vc_status"))
    {
        setValueIfChanged(status, text);
        setRowControlVisible(row("vc_status"), !text.empty());
        if (LLTextBox* line = dynamic_cast<LLTextBox*>(status))
            line->setColor(ALKitStyle::color(warning ? ALKitStyle::RECORD : ALKitStyle::TEXT));
    }
}

void ALPanelDirectorVCam::clearStatus()
{
    if (mStatus.empty()) return;
    mStatus.clear();
    mStatusWarning = false;
    if (LLUICtrl* status = control("vc_status")) setValueIfChanged(status, LLSD());
    setRowControlVisible(row("vc_status"), false);
}

void ALPanelDirectorVCam::setAspectValue(const std::string& name, F32 ratio)
{
    LLComboBox* combo = dynamic_cast<LLComboBox*>(control(name));
    if (!combo) return;
    constexpr F32 values[] = { 16.f / 9.f, 4.f / 3.f, 1.f };
    S32 nearest = 0;
    F32 distance = fabsf(ratio - values[0]);
    for (S32 index = 1; index < 3; ++index)
        if (fabsf(ratio - values[index]) < distance)
        {
            nearest = index;
            distance = fabsf(ratio - values[index]);
        }
    const bool explicitly_custom = name == "vc_output_aspect" ? mCustomAspect : mCustomScreenAspect;
    combo->setCurrentByIndex(!explicitly_custom && distance <= .001f ? nearest : 3);
    if (name == "vc_output_aspect" && distance > .001f) mCustomAspect = true;
    if (name == "vc_screen_aspect" && distance > .001f) mCustomScreenAspect = true;
}

void ALPanelDirectorVCam::updateCardState(bool have_capture, bool have_display,
                                          const std::string& camera_label)
{
    if (!mKitPage) return;
    const char* camera_cards[] = { "source", "placement", "optics", "guide",
        "camera_look", "look_goes", "output_frame", "faces", "picture_rate" };
    const std::string badge = camera_label.empty() ? "CAMERA" : camera_label;
    for (ALKitCard* card : mCards)
        card->setBadgePlaceholder("CAM n", badge);
    for (const char* id : camera_cards)
        if (ALKitCard* card = mKitPage->findCard(id))
        {
            card->setCodeVisible(have_capture);
        }
    // The skeleton card's visibility depends on the camera type and is set once per refresh in
    // refresh(); only force it hidden here when there is no camera (avoids a show/hide flip).
    if (ALKitCard* skeleton = mKitPage->findCard("skeleton"))
    {
        if (!have_capture) skeleton->setCodeVisible(false);
    }
    for (const char* id : { "face_mapping", "tv_effects" })
        if (ALKitCard* card = mKitPage->findCard(id)) card->setCodeVisible(have_display);
    if (ALKitCard* raw = mKitPage->findCard("raw_output"))
        raw->setCodeVisible(have_capture && !have_display);
}

void ALPanelDirectorVCam::onControl(LLUICtrl* source)
{
    if (!source || mRefreshing) return;
    clearStatus();
    const std::string name = source->getName();
    if (name == "vc_camera_selector") ++mMediaEditEpoch;
    const LLSD value = source->getValue();
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();

    if (capture && capture->isMedia() && name.rfind("vc_media_", 0) == 0 && name != "vc_media_running")
    {
        const auto handle = capture->mHandle;
        std::string reason;
        if (name == "vc_media_kind" || name == "vc_media_video" || name == "vc_media_play" ||
            name == "vc_media_stop" || name == "vc_media_browse")
        {
            mMediaPlayStatus.clear(); mMediaError.clear();
            mMediaRefusals.erase(handle.mId);
            LLPrismLens::clearMediaFileBrowse(handle);
            if (capture->mediaUrlData()) validateMediaVideo();
        }
        if (name == "vc_media_kind")
        {
            ++mMediaEditEpoch;
            LLPrismLens::switchMediaSourceKind(handle, value.asString() == "url");
        }
        else if (name == "vc_media_video") { ++mMediaEditEpoch; commitMediaVideo(); }
        else if (name == "vc_media_play")
        {
            if (commitMediaVideo())
            {
                if (!LLPrismLens::playMediaFile(handle))
                {
                    const auto state = LLPrismLens::registrySnapshot();
                    for (U32 i = 0; i < state.mSlotCount; ++i)
                        if (sameCapture(state.mCaptures[i].mHandle, handle))
                            mMediaPlayStatus = state.mCaptures[i].mRuntime.mReason;
                }
            }
            else LL_INFOS("VCamMedia") << "Play refused rule=invalid editor draft reason=" << mMediaError << LL_ENDL;
        }
        else if (name == "vc_media_pause") LLPrismLens::pauseMediaFile(handle);
        else if (name == "vc_media_stop") { ++mMediaEditEpoch; LLPrismLens::stopMediaFile(handle); }
        else if (name == "vc_media_browse")
        {
            const auto panel = getHandle();
            const U64 epoch = ++mMediaEditEpoch;
            LLPrismLens::browseMediaFile(handle, [panel, handle, epoch]()
            {
                auto* self = dynamic_cast<ALPanelDirectorVCam*>(panel.get());
                return self && self->mModel.pageVisible() && self->mMediaEditEpoch == epoch &&
                    sameCapture(self->mModel.selectedCapture(), handle);
            }, [panel, handle, epoch]()
            {
                auto* self = dynamic_cast<ALPanelDirectorVCam*>(panel.get());
                if (!self || self->mMediaEditEpoch != epoch ||
                    !sameCapture(self->mModel.selectedCapture(), handle)) return;
                // Force editor read-back on success, including an identical cue.
                self->mMediaRefusals.erase(handle.mId);
                self->mMediaEditHandle = {};
                self->mModel.markDirty(ALVCamPageModel::DIRTY_ALL);
            }, [panel, handle, epoch](const std::string& path, const std::string& reason)
            {
                auto* self = dynamic_cast<ALPanelDirectorVCam*>(panel.get());
                if (!self || self->mMediaEditEpoch != epoch ||
                    !sameCapture(self->mModel.selectedCapture(), handle)) return;
                self->mMediaDraft = path;
                self->refuseMediaVideo(reason);
                setValueIfChanged(self->mMediaVideo, path);
            });
        }
        else if ((name == "vc_media_loop" || name == "vc_media_volume") && capture->mediaUrlData())
        {
            if (name == "vc_media_loop") LLPrismLens::setMediaFileLoop(handle, value.asBoolean());
            else LLPrismLens::setMediaFileVolume(handle, static_cast<F32>(value.asReal()));
        }
        else if (name == "vc_media_cache_clear") LLPrismLens::clearMediaFileCache();
        else if (name == "vc_media_locate" && capture->mediaData())
            if (auto* object = gObjectList.findObject(capture->mediaData()->mObjectId))
                handle_zoom_to_object(object->getID());
        if (!reason.empty()) setStatus(reason);
        mModel.refreshNow();
        return;
    }

    if (name == "vc_pick_media_face" && capture && capture->isMedia())
    {
        const auto handle = capture->mHandle;
        const auto descriptor = capture->mediaData() ? *capture->mediaData() : LLPrismLens::MediaFaceSource();
        const U64 revision = mModel.registry().mConfigurationRevision;
        const auto panel = getHandle();
        auto& tool = MediaFacePickTool::instance();
        LLToolMgr::getInstance()->setTransientTool(&tool);
        tool.begin([panel, handle, descriptor, revision](LLViewerObject* object, S32 te)
        {
            auto* self = dynamic_cast<ALPanelDirectorVCam*>(panel.get());
            if (!self || !object || !sameCapture(self->mModel.selectedCapture(), handle) ||
                LLPrismLens::registrySnapshot().mConfigurationRevision != revision) return;
            auto source = descriptor;
            source.mObjectId = object->getID();
            source.mTE = te;
            std::string reason;
            if (!LLPrismLens::setMediaSource(handle, source, &reason)) self->setStatus(reason);
            self->mModel.refreshNow();
        });
        setStatus("Click a prim media face; Escape cancels.", false);
        return;
    }
    if (name == "vc_media_running" && capture && capture->isMedia())
        LLPrismLens::setMediaRunning(capture->mHandle, !capture->mMediaRunning);
    else if (name == "vc_enable") gSavedSettings.setBOOL("PrismLensEnabled", value.asBoolean());
    else if (name == "vc_open_manager") LLFloaterReg::showInstance("prism_manager");
    else if (name == "vc_monitor_header")
    {
        const std::string action = value["action"].asString();
        if (action == "size")
        {
            mCompact = value["value"].asBoolean();
            mLargePage = 0;
            refresh();
        }
        else if (action == "previews")
            gSavedSettings.setBOOL("PrismMonitorPreviewsEnabled", value["value"].asBoolean());
        else if (action == "info")
            gSavedSettings.setBOOL("PrismMonitorInfoOverlay", value["value"].asBoolean());
        else if (action == "speed")
            gSavedSettings.setF32("PrismMonitorPreviewFPS", static_cast<F32>(value["value"].asReal()));
    }
    else if (name == "vc_monitor_pager") { mLargePage = value.asInteger() ? 1 : 0; refresh(); }
    else if (name == "vc_lens_zoom") gSavedSettings.setF32("PrismLensZoom", static_cast<F32>(value.asReal()));
    else if (name == "vc_protect") gSavedSettings.setBOOL("PrismAdaptivePerformance", value.asBoolean());
    else if (name == "vc_main_fps") gSavedSettings.setF32("PrismProtectedMainFPS", static_cast<F32>(value.asReal()));
    else if (name == "vc_capture_budget") gSavedSettings.setF32("PrismCaptureRefreshCeilingHz", static_cast<F32>(value.asReal()));
    else if (name == "vc_quality_ceiling") gSavedSettings.setF32("PrismLensResolutionScale", static_cast<F32>(value.asReal()));
    else if (name == "vc_camera_selector")
    {
        mCustomAspect = false;
        mCustomScreenAspect = false;
        const LLUUID id = value.asUUID();
        for (U32 i = 0; i < mModel.registry().mSlotCount; ++i)
            if (mModel.registry().mCaptures[i].mHandle.mId == id)
                mModel.setSelectedCapture(mModel.registry().mCaptures[i].mHandle);
    }
    else if (name == "vc_monitor_strip" && value["action"].asString() == "menu")
        showMonitorMenu(value["slot"].asInteger(), value["x"].asInteger(), value["y"].asInteger());
    else if (name == "vc_monitor_strip" && value["action"].asString() == "drag_rejected")
        setStatus(value["reason"].asString());
    else if (name == "vc_face_list" && capture)
    {
        mCustomScreenAspect = false;
        LLPrismLens::DisplayHandle selected;
        const LLUUID id = value.asUUID();
        for (U32 i = 0; i < mModel.registry().mDisplayCount; ++i)
            if (mModel.registry().mDisplays[i].mHandle.mId == id)
                selected = mModel.registry().mDisplays[i].mHandle;
        if (mMoveTargetCapture.mId.notNull())
        {
            if (selected.mId.isNull())
            {
                mMoveTargetCapture = {};
                mFaceList->setMovePick(false);
                setStatus("Move cancelled.", false);
                return;
            }
            const LLPrismLens::ActionStatus status =
                ALVCamPageModel::reassignDisplayStatus(selected, mMoveTargetCapture);
            if (!status.allowed())
            {
                setStatus(status.mReason);
                return;
            }
            std::string reason;
            if (LLPrismLens::reassignDisplay(selected, mMoveTargetCapture, &reason) !=
                LLPrismLens::ERegistryResult::OK)
            {
                setStatus(reason);
                return;
            }
            mMoveTargetCapture = {};
            mFaceList->setMovePick(false);
            setStatus("Moved the selected face to this camera.", false);
            return;
        }
        mModel.setSelectedDisplay(selected);
    }
    else if (name == "vc_projector_list")
    {
        const LLUUID id = value["projector"].asUUID();
        for (U32 index = 0; index < mModel.registry().mProjectorCount; ++index)
        {
            const LLPrismLens::ProjectorDefinition& projector =
                mModel.registry().mProjectors[index];
            if (projector.mHandle.mId != id) continue;
            if (value["action"].asString() == "select")
            {
                mModel.setSelectedProjector(projector.mHandle);
                break;
            }
            std::string reason;
            if (!mModel.removeProjector(projector.mHandle, &reason)) setStatus(reason);
            else setStatus("Projector feed removed.", false);
            break;
        }
    }
    else if (name == "vc_add_camera")
    {
        std::string reason;
        const std::string action = value.asString();
        if (action == "object") mModel.addFromSelectedObject(&reason);
        else if (action == "virtual") mModel.addVirtualAtView(&reason);
        else if (action == "ots") mModel.addOtsPair(&reason);
        else if (action == "lens") mModel.addSurfaceLens(&reason);
        else if (action == "media" && LLPrismLens::mediaSlotsEnabled())
            mModel.addMediaSlot(&reason);
        if (!reason.empty()) setStatus(reason);
        // Snap back to "+ Add camera..." (item 0). Its value is undefined, so
        // setValue(LLSD()) matched nothing and the last choice stayed shown.
        if (LLComboBox* add_combo = dynamic_cast<LLComboBox*>(source))
            add_combo->selectFirstItem();
        else
            source->setValue(LLSD());
    }
    else if (name == "vc_remove_capture" && capture)
        confirmRemoveCapture(*capture);
    else if (name == "vc_set_selected_camera" && capture)
    {
        std::string reason;
        if (!LLPrismLens::setSelectedCamera(capture->mHandle, &reason)) setStatus(reason);
    }
    else if (name == "vc_snap_view" && capture && capture->cameraData())
    {
        LLPrismLens::CameraSettings settings = capture->cameraData()->mSettings;
        ALVCamPageModel::currentViewTransform(settings.mVirtualPos, settings.mVirtualRot);
        settings.mVirtual = true;
        mModel.setCamera(capture->mHandle, settings);
    }
    else if ((name == "vc_lens_locate" || name == "vc_face_locate") && capture)
    {
        const LLPrismLens::DisplayDefinition* target = display;
        if (name == "vc_lens_locate")
            for (U32 i = 0; i < mModel.registry().mDisplayCount; ++i)
                if (sameCapture(mModel.registry().mDisplays[i].mCapture, capture->mHandle))
                {
                    target = &mModel.registry().mDisplays[i];
                    break;
                }
        LLViewerObject* object = target && !target->mSettings.mVirtual
            ? gObjectList.findObject(target->mDisplayObjectId) : nullptr;
        if (!object) setStatus("That face is not currently available in-world.");
        else
        {
            LLSelectMgr::getInstance()->deselectAll();
            LLSelectMgr::getInstance()->selectObjectOnly(object, target->mDisplayTextureEntry);
            handle_zoom_to_object(object->getID());
            setStatus("Selected and framed that face in-world.", false);
        }
    }
    else if (name == "vc_output_aspect")
    {
        mCustomAspect = static_cast<F32>(value.asReal()) < 0.0;
        if (!mCustomAspect) commitCamera(name, value);
        refresh();
    }
    else if (name == "vc_screen_aspect")
    {
        mCustomScreenAspect = static_cast<F32>(value.asReal()) < 0.f;
        if (!mCustomScreenAspect) commitDisplay(name, value);
        refresh();
    }
    else if (name == "vc_go_output" && mKitPage) mKitPage->setChipIndex(3);
    else if (name == "vc_add_face" && capture)
    {
        std::string reason;
        if (mModel.bindSelectedFace(capture->mHandle, mNewFaceFit, &reason) != LLPrismLens::ERegistryResult::OK)
            setStatus(reason);
    }
    else if (name == "vc_new_face_fit")
    {
        mNewFaceFit = static_cast<LLPrismLens::EFitMode>(llclamp(value.asInteger(), 0, 2));
        mMonitorStrip->setNewFaceFit(mNewFaceFit);
    }
    else if (name == "vc_add_screen" && capture)
    {
        LLVector3 pos;
        LLQuaternion rot;
        ALVCamPageModel::currentViewTransform(pos, rot);
        pos += LLVector3(0.f, 0.f, -3.f) * rot;
        std::string reason;
        if (mModel.addVirtualScreen(capture->mHandle, pos, rot, 1.6f, .9f, &reason) != LLPrismLens::ERegistryResult::OK)
            setStatus(reason);
    }
    else if (name == "vc_screen_reposition" && display && display->mSettings.mVirtual)
    {
        LLVector3 view_pos;
        LLQuaternion rot;
        ALVCamPageModel::currentViewTransform(view_pos, rot);
        LLPrismLens::DisplaySettings settings = display->mSettings;
        settings.mPos = view_pos + LLVector3(0.f, 0.f, -3.f) * rot;
        settings.mRot = rot;
        std::string reason;
        if (!mModel.setDisplay(display->mHandle, settings, &reason)) setStatus(reason);
    }
    else if (name == "vc_remove_face" && display)
    {
        std::string reason;
        if (!LLPrismLens::removeDisplay(display->mHandle, &reason)) setStatus(reason);
    }
    else if (name == "vc_face_feed" && display && capture)
    {
        std::string reason;
        if (LLPrismLens::setDisplayGateSubscribed(display->mHandle,
                value.asInteger() == 1, &capture->mHandle, &reason) != LLPrismLens::ERegistryResult::OK)
            setStatus(reason);
    }
    else if (name == "vc_bone_reset" && capture && capture->cameraData())
    {
        LLPrismLens::CameraSettings settings = capture->cameraData()->mSettings;
        settings.mBonePov = LLPrismLens::BonePovSettings();
        settings.mBonePov.mEnabled = settings.mVirtual;
        mModel.setCamera(capture->mHandle, settings);
    }
    else if (name == "vc_source_type" && capture && capture->isCameraFeed())
    {
        const S32 source_type = value.asInteger();
        LLPrismLens::CameraSettings settings = capture->cameraData()->mSettings;
        if (source_type == 0)
        {
            std::string reason;
            if (!LLPrismLens::setSelectedCamera(capture->mHandle, &reason)) setStatus(reason);
        }
        else
        {
            settings.mTestPattern = source_type == 3;
            // Keep a stored virtual transform across a Test detour; start at the
            // current view when none was ever stored (Object -> Test -> Virtual).
            if (!settings.mTestPattern && !settings.mVirtual &&
                (!capture->cameraData()->mSettings.mTestPattern || settings.mVirtualPos.isExactlyZero()))
                ALVCamPageModel::currentViewTransform(settings.mVirtualPos, settings.mVirtualRot);
            settings.mVirtual = !settings.mTestPattern;
            settings.mBonePov.mEnabled = !settings.mTestPattern && source_type == 2;
            std::string reason;
            if (!mModel.setCamera(capture->mHandle, settings, &reason)) setStatus(reason);
        }
        refresh(ALVCamPageModel::DIRTY_CONFIGURATION);
    }
    else if (name == "vc_rate_preset" && capture && value.asReal() > 0.0)
    {
        LLPrismLens::CaptureRateSettings rate = capture->mRate;
        rate.mMode = LLPrismLens::EOutputRateMode::TARGET_FPS;
        rate.mTargetFps = static_cast<F32>(value.asReal());
        mModel.setRate(capture->mHandle, rate);
        source->setValue(LLSD(0));
    }
    else if (name.rfind("vc_proj_", 0) == 0)
        commitProjector(name, value);
    else if (name.rfind("vc_tv_", 0) == 0 || name.rfind("vc_face_", 0) == 0 ||
             name.rfind("vc_anchor_", 0) == 0 || name.rfind("vc_bar_", 0) == 0 ||
             name.rfind("vc_screen_", 0) == 0)
        commitDisplay(name, value);
    else
        commitCamera(name, value);
}

void ALPanelDirectorVCam::commitCamera(const std::string& name, const LLSD& value)
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture) return;
    if (name == "vc_camera_name")
    {
        std::string reason;
        if (!mModel.setLabel(capture->mHandle, value.asString(), &reason))
            setStatus(reason);
        else
        {
            mModel.refreshNow(ALVCamPageModel::DIRTY_CONFIGURATION);
            if (const LLPrismLens::CaptureDefinition* renamed = selectedCapture())
                setValueIfChanged(mCameraName, renamed->mLabel);
        }
        return;
    }
    if (capture->isMedia())
    {
        if (name == "vc_source_aspect")
        {
            LLPrismLens::setMediaSourceAspect(capture->mHandle,
                static_cast<LLPrismLens::ESourceAspect>(llclamp(value.asInteger(), 0, 3)));
            mModel.refreshNow();
        }
        else if (name == "vc_rate_mode" || name == "vc_target_fps")
        {
            auto rate = capture->mRate;
            if (name == "vc_rate_mode") rate.mMode = static_cast<LLPrismLens::EOutputRateMode>(llclamp(value.asInteger(), 0, 1));
            else rate.mTargetFps = static_cast<F32>(value.asReal());
            mModel.setRate(capture->mHandle, rate);
        }
        return;
    }
    LLPrismLens::CameraSettings settings = capture->cameraData()->mSettings;
    LLPrismLens::CaptureRateSettings rate = capture->mRate;
    bool camera_changed = true;
    if (name == "vc_eye_x") settings.mLocalEyeOffset.mV[0] = static_cast<F32>(value.asReal());
    else if (name == "vc_eye_y") settings.mLocalEyeOffset.mV[1] = static_cast<F32>(value.asReal());
    else if (name == "vc_eye_z") settings.mLocalEyeOffset.mV[2] = static_cast<F32>(value.asReal());
    else if (name == "vc_fov_mode") settings.mFovMode = static_cast<LLPrismLens::EFovMode>(llclamp(value.asInteger(), 0, 1));
    else if (name == "vc_fov_deg") settings.mFixedVerticalFovRad = static_cast<F32>(value.asReal()) * DEG_TO_RAD;
    else if (name == "vc_fov_attached") settings.mBonePov.mFovDeg = static_cast<F32>(value.asReal());
    else if (name == "vc_near_clip") settings.mNearClip = static_cast<F32>(value.asReal());
    else if (name == "vc_far_clip") settings.mFarClip = static_cast<F32>(value.asReal());
    else if (name == "vc_guide_show") settings.mShowGuide = value.asBoolean();
    else if (name == "vc_guide_thirds") settings.mGuideThirds = value.asBoolean();
    else if (name == "vc_guide_up") settings.mGuideUpRoll = value.asBoolean();
    else if (name == "vc_guide_cross") settings.mGuideCrosshair = value.asBoolean();
    else if (name == "vc_guide_clip") settings.mGuideClipMarkers = value.asBoolean();
    else if (name == "vc_exposure") settings.mOptics.mExposureBias = static_cast<F32>(value.asReal());
    else if (name == "vc_grain") settings.mOptics.mFilmGrain = static_cast<F32>(value.asReal());
    else if (name == "vc_chromatic") settings.mOptics.mChromaticAberration = static_cast<F32>(value.asReal());
    else if (name == "vc_camera_crt") settings.mOptics.mCRTScanlines = static_cast<F32>(value.asReal());
    else if (name == "vc_output_aspect" || name == "vc_output_custom")
        settings.mOutputAspect = ratioFromValue(value, settings.mOutputAspect);
    else if (name == "vc_bone_anchor") settings.mBonePov.mAnchorSlot = value.asInteger();
    else if (name == "vc_bone_joint")
    {
        settings.mBonePov.mCustomJoint = value.asString();
        settings.mBonePov.mJointSelection = settings.mBonePov.mCustomJoint.empty()
            ? LLPrismLens::BONE_JOINT_EYELINE : LLPrismLens::BONE_JOINT_NAMED;
        if (!LLPrismLens::isBonePovSpineJoint(settings.mBonePov.mCustomJoint))
            settings.mBonePov.mAimMode = LLPrismLens::BONE_AIM_STABILIZED;
    }
    else if (name == "vc_bone_aim") settings.mBonePov.mAimMode = value.asInteger();
    else if (name == "vc_bone_roll") settings.mBonePov.mRollMode = value.asInteger();
    else if (name == "vc_bone_smooth") settings.mBonePov.mSmoothingSec = static_cast<F32>(value.asReal());
    else if (name == "vc_bone_x") settings.mBonePov.mOffset.mV[0] = static_cast<F32>(value.asReal());
    else if (name == "vc_bone_y") settings.mBonePov.mOffset.mV[1] = static_cast<F32>(value.asReal());
    else if (name == "vc_bone_z") settings.mBonePov.mOffset.mV[2] = static_cast<F32>(value.asReal());
    else if (name == "vc_bone_pitch") settings.mBonePov.mTrimPitchDeg = static_cast<F32>(value.asReal());
    else if (name == "vc_bone_yaw") settings.mBonePov.mTrimYawDeg = static_cast<F32>(value.asReal());
    else if (name == "vc_bone_scale") settings.mBonePov.mScaleAware = value.asBoolean();
    else if (name == "vc_bone_enabled") settings.mBonePov.mEnabled = value.asBoolean() && settings.mVirtual;
    else if (name == "vc_rate_mode")
    {
        rate.mMode = static_cast<LLPrismLens::EOutputRateMode>(llclamp(value.asInteger(), 0, 1));
        camera_changed = false;
        mModel.setRate(capture->mHandle, rate);
    }
    else if (name == "vc_target_fps")
    {
        rate.mTargetFps = static_cast<F32>(value.asReal());
        camera_changed = false;
        mModel.setRate(capture->mHandle, rate);
    }
    else camera_changed = false;
    if (camera_changed) mModel.setCamera(capture->mHandle, settings);
}

void ALPanelDirectorVCam::commitDisplay(const std::string& name, const LLSD& value)
{
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    if (!display) return;
    LLPrismLens::DisplaySettings settings = display->mSettings;
    LLPrismLens::ScreenEffects& effect = settings.mEffects;
    bool changed = true;
    if (name == "vc_face_fit") settings.mFitMode = static_cast<LLPrismLens::EFitMode>(llclamp(value.asInteger(), 0, 2));
    else if (name == "vc_anchor_x") settings.mAnchor[0] = static_cast<F32>(value.asReal());
    else if (name == "vc_anchor_y") settings.mAnchor[1] = static_cast<F32>(value.asReal());
    else if (name == "vc_screen_height") { const F32 ratio = settings.mWidth / llmax(.01f, settings.mHeight); settings.mHeight = static_cast<F32>(value.asReal()); settings.mWidth = settings.mHeight * ratio; }
    else if (name == "vc_screen_aspect") settings.mWidth = settings.mHeight * ratioFromValue(value, settings.mWidth / llmax(.01f, settings.mHeight));
    else if (name == "vc_bar_r") settings.mBarColorLinear[0] = static_cast<F32>(value.asReal());
    else if (name == "vc_bar_g") settings.mBarColorLinear[1] = static_cast<F32>(value.asReal());
    else if (name == "vc_bar_b") settings.mBarColorLinear[2] = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_scan") effect.mScanlines = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_gray") effect.mGrayscale = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_static") effect.mStatic = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_vignette") effect.mVignette = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_brightness") effect.mBrightness = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_flip_h") effect.mFlipH = value.asBoolean();
    else if (name == "vc_tv_flip_v") effect.mFlipV = value.asBoolean();
    else if (name == "vc_tv_rotate") effect.mRotate90 = value.asBoolean();
    else if (name == "vc_tv_lines") effect.mScanlineCount = (static_cast<F32>(value.asReal()) - 100.f) / 500.f;
    else if (name == "vc_tv_dark") effect.mScanlineDepth = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_tv_sharp") effect.mScanlineSharpness = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_tv_thick") effect.mScanlineThickness = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_tv_pixel") effect.mPixelate = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_sepia") effect.mSepia = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_chroma") effect.mChromaBleed = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_vroll") effect.mVerticalRoll = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_roll_speed") effect.mRollSpeed = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_tracking") effect.mTracking = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_flicker") effect.mFlicker = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_interlace") effect.mInterlace = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_dropout") effect.mDropout = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_sheen") effect.mSheen = static_cast<F32>(value.asReal());
    else if (name == "vc_tv_preset")
    {
        const std::string preset = value.asString();
        // "clean" is a real preset: it resets every TV effect.
        if (preset != "clean" && preset != "crt" && preset != "broken" &&
            preset != "vhs" && preset != "security")
        {
            return;
        }
        effect = LLPrismLens::ScreenEffects();
        if (preset == "crt")
        {
            effect.mScanlines = .6f;
            effect.mScanlineCount = .5f;
            effect.mVignette = .4f;
            effect.mFlicker = .15f;
        }
        else if (preset == "broken")
        {
            effect.mGrayscale = 1.f;
            effect.mStatic = .35f;
            effect.mVerticalRoll = .5f;
            effect.mRollSpeed = .3f;
            effect.mScanlines = .8f;
            effect.mDropout = .4f;
        }
        else if (preset == "vhs")
        {
            effect.mTracking = .45f;
            effect.mChromaBleed = .6f;
            effect.mInterlace = .3f;
            effect.mFlicker = .2f;
        }
        else if (preset == "security")
        {
            effect.mGrayscale = 1.f;
            effect.mScanlines = .4f;
            effect.mStatic = .15f;
            effect.mVignette = .5f;
            effect.mPixelate = .2f;
        }
        if (LLUICtrl* preset_control = control("vc_tv_preset")) preset_control->setValue(LLSD());
    }
    else changed = false;
    if (changed) mModel.setDisplay(display->mHandle, settings);
}

void ALPanelDirectorVCam::commitProjector(const std::string& name,
                                          const LLSD& value)
{
    const LLPrismLens::ProjectorDefinition* projector = selectedProjector();
    if (!projector) return;
    if (name == "vc_proj_look")
    {
        const S32 preset = value.asInteger();
        if (preset >= 0)
        {
            std::string reason;
            if (!mModel.setProjectorLook(projector->mHandle,
                    static_cast<ALVCamPageModel::EProjectorLookPreset>(preset),
                    &reason))
                setStatus(reason);
        }
        return;
    }

    LLPrismLens::ProjectorSettings settings = projector->mSettings;
    LLPrismLens::ProjectorLook& look = settings.mLook;
    LLPrismLens::ScreenEffects& effect = settings.mEffects;
    bool changed = true;
    if (name == "vc_proj_fit") settings.mFitMode =
        static_cast<LLPrismLens::EFitMode>(llclamp(value.asInteger(), 0, 2));
    else if (name == "vc_proj_anchor_x") settings.mAnchor[0] = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_anchor_y") settings.mAnchor[1] = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_flip_h") settings.mFlipH = value.asBoolean();
    else if (name == "vc_proj_flip_v") settings.mFlipV = value.asBoolean();
    else if (name == "vc_proj_rotate") settings.mRotate = static_cast<U8>(llclamp(value.asInteger(), 0, 3));
    else if (name == "vc_proj_keystone_h") look.mKeystoneH = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_keystone_v") look.mKeystoneV = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_throw") look.mThrowMode =
        static_cast<LLPrismLens::EProjectorThrow>(llclamp(value.asInteger(), 0, 2));
    else if (name == "vc_proj_brightness") settings.mFeedEV = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_lamp_power") settings.mLampPowerEV = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_follow_exposure") settings.mFollowMainExposure = value.asBoolean();
    else if (name == "vc_proj_lamp_kelvin") look.mLampKelvin = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_gel_tint") look.mGelTint = LLColor3(value);
    else if (name == "vc_proj_hotspot") look.mHotspot = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_shutter_flicker") look.mShutterFlicker = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_shutter_rate") look.mShutterHz = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_gate_weave") look.mGateWeave = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_grain") look.mGrain = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_dust") look.mDust = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_scratches") look.mScratches = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_burn") look.mBurn = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_halation") look.mHalation = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_resolution") settings.mResolution = static_cast<U16>(value.asInteger());
    else if (name == "vc_proj_no_signal") settings.mNoSignal =
        static_cast<LLPrismLens::ENoSignal>(llclamp(value.asInteger(), 0, 2));
    else if (name == "vc_proj_keep_gobo") settings.mKeepGobo = value.asBoolean();
    else if (name == "vc_proj_tv_scan") effect.mScanlines = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_gray") effect.mGrayscale = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_static") effect.mStatic = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_vignette") effect.mVignette = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_brightness") effect.mBrightness = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_lines") effect.mScanlineCount =
        (static_cast<F32>(value.asReal()) - 100.f) / 500.f;
    else if (name == "vc_proj_tv_dark") effect.mScanlineDepth = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_proj_tv_sharp") effect.mScanlineSharpness = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_proj_tv_thick") effect.mScanlineThickness = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_proj_tv_pixel") effect.mPixelate = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_sepia") effect.mSepia = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_chroma") effect.mChromaBleed = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_vroll") effect.mVerticalRoll = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_roll_speed") effect.mRollSpeed = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_tracking") effect.mTracking = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_flicker") effect.mFlicker = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_interlace") effect.mInterlace = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_dropout") effect.mDropout = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_sheen") effect.mSheen = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_preset")
    {
        const std::string preset = value.asString();
        // "clean" is a real preset: it resets every TV effect.
        if (preset != "clean" && preset != "crt" && preset != "broken" &&
            preset != "vhs" && preset != "security")
        {
            return;
        }
        effect = LLPrismLens::ScreenEffects();
        if (preset == "crt")
        {
            effect.mScanlines = .6f; effect.mScanlineCount = .5f;
            effect.mVignette = .4f; effect.mFlicker = .15f;
        }
        else if (preset == "broken")
        {
            effect.mGrayscale = 1.f; effect.mStatic = .35f;
            effect.mVerticalRoll = .5f; effect.mRollSpeed = .3f;
            effect.mScanlines = .8f; effect.mDropout = .4f;
        }
        else if (preset == "vhs")
        {
            effect.mTracking = .45f; effect.mChromaBleed = .6f;
            effect.mInterlace = .3f; effect.mFlicker = .2f;
        }
        else if (preset == "security")
        {
            effect.mGrayscale = 1.f; effect.mScanlines = .4f;
            effect.mStatic = .15f; effect.mVignette = .5f;
            effect.mPixelate = .2f;
        }
        if (LLUICtrl* preset_control = control("vc_proj_tv_preset"))
            preset_control->setValue(LLSD());
    }
    else changed = false;

    if (changed)
    {
        std::string reason;
        if (!mModel.setProjectorSettings(projector->mHandle, settings, &reason))
            setStatus(reason);
    }
}

bool ALPanelDirectorVCam::applyCameraPasteValue(
    const std::string& name, const LLSD& value,
    LLPrismLens::CameraSettings& settings)
{
    if (name == "vc_fov_mode") settings.mFovMode = static_cast<LLPrismLens::EFovMode>(llclamp(value.asInteger(), 0, 1));
    else if (name == "vc_fov_deg") settings.mFixedVerticalFovRad = static_cast<F32>(value.asReal()) * DEG_TO_RAD;
    else if (name == "vc_fov_attached") settings.mBonePov.mFovDeg = static_cast<F32>(value.asReal());
    else if (name == "vc_near_clip") settings.mNearClip = static_cast<F32>(value.asReal());
    else if (name == "vc_far_clip") settings.mFarClip = static_cast<F32>(value.asReal());
    else if (name == "vc_exposure") settings.mOptics.mExposureBias = static_cast<F32>(value.asReal());
    else if (name == "vc_grain") settings.mOptics.mFilmGrain = static_cast<F32>(value.asReal());
    else if (name == "vc_chromatic") settings.mOptics.mChromaticAberration = static_cast<F32>(value.asReal());
    else if (name == "vc_camera_crt") settings.mOptics.mCRTScanlines = static_cast<F32>(value.asReal());
    else if (name == "vc_output_aspect" || name == "vc_output_custom")
        settings.mOutputAspect = ratioFromValue(value, settings.mOutputAspect);
    else return false;
    return true;
}

bool ALPanelDirectorVCam::applyProjectorPasteValue(
    const std::string& name, const LLSD& value,
    LLPrismLens::ProjectorSettings& settings)
{
    LLPrismLens::ProjectorLook& look = settings.mLook;
    LLPrismLens::ScreenEffects& effect = settings.mEffects;
    if (name == "vc_proj_fit") settings.mFitMode = static_cast<LLPrismLens::EFitMode>(llclamp(value.asInteger(), 0, 2));
    else if (name == "vc_proj_anchor_x") settings.mAnchor[0] = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_anchor_y") settings.mAnchor[1] = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_flip_h") settings.mFlipH = value.asBoolean();
    else if (name == "vc_proj_flip_v") settings.mFlipV = value.asBoolean();
    else if (name == "vc_proj_rotate") settings.mRotate = static_cast<U8>(llclamp(value.asInteger(), 0, 3));
    else if (name == "vc_proj_keystone_h") look.mKeystoneH = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_keystone_v") look.mKeystoneV = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_throw") look.mThrowMode = static_cast<LLPrismLens::EProjectorThrow>(llclamp(value.asInteger(), 0, 2));
    else if (name == "vc_proj_brightness") settings.mFeedEV = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_lamp_power") settings.mLampPowerEV = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_follow_exposure") settings.mFollowMainExposure = value.asBoolean();
    else if (name == "vc_proj_lamp_kelvin") look.mLampKelvin = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_gel_tint") look.mGelTint = LLColor3(value);
    else if (name == "vc_proj_hotspot") look.mHotspot = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_shutter_flicker") look.mShutterFlicker = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_shutter_rate") look.mShutterHz = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_gate_weave") look.mGateWeave = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_grain") look.mGrain = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_dust") look.mDust = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_scratches") look.mScratches = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_burn") look.mBurn = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_halation") look.mHalation = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_resolution") settings.mResolution = static_cast<U16>(value.asInteger());
    else if (name == "vc_proj_no_signal") settings.mNoSignal = static_cast<LLPrismLens::ENoSignal>(llclamp(value.asInteger(), 0, 2));
    else if (name == "vc_proj_keep_gobo") settings.mKeepGobo = value.asBoolean();
    else if (name == "vc_proj_tv_scan") effect.mScanlines = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_gray") effect.mGrayscale = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_static") effect.mStatic = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_vignette") effect.mVignette = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_brightness") effect.mBrightness = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_lines") effect.mScanlineCount = (static_cast<F32>(value.asReal()) - 100.f) / 500.f;
    else if (name == "vc_proj_tv_dark") effect.mScanlineDepth = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_proj_tv_sharp") effect.mScanlineSharpness = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_proj_tv_thick") effect.mScanlineThickness = static_cast<F32>(value.asReal()) / 100.f;
    else if (name == "vc_proj_tv_pixel") effect.mPixelate = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_sepia") effect.mSepia = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_chroma") effect.mChromaBleed = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_vroll") effect.mVerticalRoll = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_roll_speed") effect.mRollSpeed = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_tracking") effect.mTracking = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_flicker") effect.mFlicker = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_interlace") effect.mInterlace = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_dropout") effect.mDropout = static_cast<F32>(value.asReal());
    else if (name == "vc_proj_tv_sheen") effect.mSheen = static_cast<F32>(value.asReal());
    else return false;
    return true;
}

bool ALPanelDirectorVCam::pasteCardValues(const std::string& card,
                                          const LLSD& values)
{
    if (!values.isArray())
        return false;
    if (card == "optics" || card == "camera_look" || card == "output_frame")
    {
        const LLPrismLens::CaptureDefinition* capture = selectedCapture();
        if (!capture || !capture->cameraData())
            return false;
        LLPrismLens::CameraSettings settings = capture->cameraData()->mSettings;
        bool changed = false;
        for (LLSD::array_const_iterator it = values.beginArray(); it != values.endArray(); ++it)
            changed = applyCameraPasteValue((*it)["key"].asString(), (*it)["value"], settings) || changed;
        return changed && mModel.setCamera(capture->mHandle, settings);
    }
    if (card == "projector" || card == "projector_tv")
    {
        const LLPrismLens::ProjectorDefinition* projector = selectedProjector();
        if (!projector)
            return false;
        LLPrismLens::ProjectorSettings settings = projector->mSettings;
        bool changed = false;
        for (LLSD::array_const_iterator it = values.beginArray(); it != values.endArray(); ++it)
            changed = applyProjectorPasteValue((*it)["key"].asString(), (*it)["value"], settings) || changed;
        if (!changed)
            return false;
        std::string reason;
        if (!mModel.setProjectorSettings(projector->mHandle, settings, &reason))
        {
            setStatus(reason);
            return false;
        }
        return true;
    }
    return false;
}

void ALPanelDirectorVCam::showMonitorMenu(U32 slot, S32 x, S32 y)
{
    mMenuCapture = LLPrismLens::CaptureHandle();
    for (U32 i = 0; i < mModel.registry().mSlotCount; ++i)
        if (mModel.registry().mCaptures[i].mSlot == slot)
            mMenuCapture = mModel.registry().mCaptures[i].mHandle;
    LLContextMenu* menu = mMonitorMenu.get();
    if (!menu || mMenuCapture.mId.isNull()) return;
    const LLPrismLens::ActionStatus bind_status =
        mModel.bindSelectedFaceStatus(mMenuCapture);
    if (LLMenuItemGL* bind = menu->findChild<LLMenuItemGL>("bind_selected_face"))
    {
        bind->setEnabled(bind_status.allowed());
        bind->setLabel(LLStringExplicit(bind_status.allowed() ? "Bind selected face" :
            std::string("Bind selected face — ") + bind_status.mReason));
        bind->setToolTip(LLStringExplicit(bind_status.allowed()
            ? std::string("Bind the selected in-world face.") : bind_status.mReason));
    }
    mMenuDisplays.clear();
    for (U32 index = 0; index < mModel.registry().mDisplayCount; ++index)
    {
        const LLPrismLens::DisplayDefinition& display = mModel.registry().mDisplays[index];
        if (sameCapture(display.mCapture, mMenuCapture)) mMenuDisplays.push_back(display.mHandle);
    }
    for (U32 index = 0; index < 16; ++index)
        if (LLMenuItemGL* choice = menu->findChild<LLMenuItemGL>(llformat("display_%u", index)))
        {
            const bool available = index < mMenuDisplays.size();
            choice->setVisible(available);
            if (available)
                for (U32 global = 0; global < mModel.registry().mDisplayCount; ++global)
                    if (sameDisplay(mModel.registry().mDisplays[global].mHandle, mMenuDisplays[index]))
                    {
                        choice->setLabel(LLStringExplicit(
                            displayName(mModel.registry().mDisplays[global], global)));
                        break;
                    }
        }
    // "Remove output" submenu: this camera's outputs (same names as the Display
    // list), then every Gate-subscribed face, which belongs to no one camera.
    mMenuRemoveDisplays.clear();
    for (U32 index = 0; index < mModel.registry().mDisplayCount; ++index)
    {
        const LLPrismLens::DisplayDefinition& display = mModel.registry().mDisplays[index];
        if (!display.mGateSubscribed && sameCapture(display.mCapture, mMenuCapture))
            mMenuRemoveDisplays.push_back(display.mHandle);
    }
    for (U32 index = 0; index < mModel.registry().mDisplayCount; ++index)
        if (mModel.registry().mDisplays[index].mGateSubscribed)
            mMenuRemoveDisplays.push_back(mModel.registry().mDisplays[index].mHandle);
    for (U32 index = 0; index < 16; ++index)
        if (LLMenuItemGL* choice = menu->findChild<LLMenuItemGL>(llformat("remove_output_%u", index)))
        {
            const bool available = index < mMenuRemoveDisplays.size();
            choice->setVisible(available);
            if (available)
                for (U32 global = 0; global < mModel.registry().mDisplayCount; ++global)
                    if (sameDisplay(mModel.registry().mDisplays[global].mHandle, mMenuRemoveDisplays[index]))
                    {
                        choice->setLabel(LLStringExplicit(faceListName(
                            mModel.registry().mDisplays[global], global)));
                        break;
                    }
        }
    if (LLMenuItemGL* none = menu->findChild<LLMenuItemGL>("remove_output_none"))
    {
        none->setVisible(mMenuRemoveDisplays.empty());
        none->setEnabled(false);
    }
    menu->buildDrawLabels();
    menu->updateParent(LLMenuGL::sMenuContainer);
    LLMenuGL::showPopup(mMonitorStrip, menu, x, y);
}

void ALPanelDirectorVCam::confirmRemoveCapture(const LLPrismLens::CaptureDefinition& capture)
{
    LLSD args;
    args["MESSAGE"] = llformat("Remove %s and all %u of its face bindings?",
                                mCameraLabels[capture.mSlot].c_str(),
                                capture.mDisplayCount);
    const LLPrismLens::CaptureHandle handle = capture.mHandle;
    LLHandle<LLPanel> panel_handle = getHandle();
    LLNotificationsUtil::add("GenericAlertYesCancel", args, LLSD(),
        [panel_handle, handle](const LLSD& notification, const LLSD& response)
        {
            ALPanelDirectorVCam* self = dynamic_cast<ALPanelDirectorVCam*>(panel_handle.get());
            if (self && LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                self->mModel.removeCapture(handle);
        });
}

void ALPanelDirectorVCam::confirmRemoveOutput(const LLPrismLens::CaptureDefinition& capture,
                                              const LLPrismLens::DisplayHandle& display,
                                              const std::string& output_name)
{
    LLSD args;
    args["MESSAGE"] = llformat("Remove output %s from %s? The camera keeps running.",
                                output_name.c_str(),
                                mCameraLabels[capture.mSlot].c_str());
    LLHandle<LLPanel> panel_handle = getHandle();
    LLNotificationsUtil::add("GenericAlertYesCancel", args, LLSD(),
        [panel_handle, display](const LLSD& notification, const LLSD& response)
        {
            ALPanelDirectorVCam* self = dynamic_cast<ALPanelDirectorVCam*>(panel_handle.get());
            if (!self || LLNotificationsUtil::getSelectedOption(notification, response) != 0)
                return;
            // The output may have been removed while the prompt was open.
            const LLPrismLens::RegistrySnapshot& registry = self->mModel.registry();
            for (U32 i = 0; i < registry.mDisplayCount; ++i)
                if (sameDisplay(registry.mDisplays[i].mHandle, display))
                {
                    std::string reason;
                    if (!LLPrismLens::removeDisplay(display, &reason)) self->setStatus(reason);
                    return;
                }
        });
}

void ALPanelDirectorVCam::onMonitorMenu(const std::string& action,
                                        const LLSD& parameter)
{
    if (mMenuCapture.mId.isNull()) return;
    clearStatus();
    if (action == "edit") mModel.setSelectedCapture(mMenuCapture);
    else if (action == "send")
    {
        std::string reason;
        if (mModel.bindSelectedFace(mMenuCapture, mNewFaceFit, &reason) !=
            LLPrismLens::ERegistryResult::OK) setStatus(reason);
    }
    else if (action == "move")
    {
        if (mKitPage) mKitPage->setChipIndex(3);
        mMoveTargetCapture = mMenuCapture;
        mFaceList->setMovePick(true);
        setStatus("Pick a face row to move it to this camera. Pick raw output to cancel.", false);
    }
    else if (action == "display")
    {
        if (parameter.asString() == "raw")
            mModel.setMonitorDisplay(mMenuCapture, {});
        else
        {
            const S32 index = parameter.asInteger();
            if (index >= 0 && index < static_cast<S32>(mMenuDisplays.size()))
                mModel.setMonitorDisplay(mMenuCapture, mMenuDisplays[index]);
        }
    }
    else if (action == "remove_camera" || action == "remove_output")
    {
        const LLPrismLens::CaptureDefinition* capture = nullptr;
        for (U32 i = 0; i < mModel.registry().mSlotCount; ++i)
            if (sameCapture(mModel.registry().mCaptures[i].mHandle, mMenuCapture))
                capture = &mModel.registry().mCaptures[i];
        if (!capture) return;
        if (action == "remove_camera")
            confirmRemoveCapture(*capture);
        else
        {
            const S32 index = parameter.asInteger();
            if (index < 0 || index >= static_cast<S32>(mMenuRemoveDisplays.size())) return;
            for (U32 global = 0; global < mModel.registry().mDisplayCount; ++global)
                if (sameDisplay(mModel.registry().mDisplays[global].mHandle, mMenuRemoveDisplays[index]))
                {
                    confirmRemoveOutput(*capture, mMenuRemoveDisplays[index],
                        faceListName(mModel.registry().mDisplays[global], global));
                    break;
                }
        }
    }
}
