/**
 * @file alpanelghoststudio.cpp
 * @brief Shared Ghost Studio panel -- see alpanelghoststudio.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelghoststudio.h"
#include "llviewermenufile.h" // [AutoAnimate]
#include "llfilepicker.h" // [AutoAnimate]
#include "alscrollfocus.h"

#include "alcompassdial.h"          // live direct-manipulation heading control
#include "alghostanimassetindex.h"
#include "alghoststudio.h"
#include "altoolcrowdplace.h"
#include "altoolghostedit.h"        // [R2-3] persistent in-world edit mode
#include "altoolghostplace.h"
#include "llactormover.h"           // actorPathColor naming consistency (style ids)
#include "llagent.h"                // agent <-> global conversion (position spinners)
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"                  // [FileSafety] sibling path splitting
#include "llfile.h"                 // [FileSafety] LLFile::exists
#include "llnotificationsutil.h"    // [FileSafety] skipped-sibling notice
#include "llf32uictrl.h"
#include "lldirectorcast.h"         // source picker = the cast
#include "lljoint.h"
#include "lllineeditor.h"
#include "llghostavatar.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llworld.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp ("To me" snap)

#include <set>
#include <utility>

// both the standalone Ghost Studio floater and the Director Console Ghosts tab
// embed this via <panel class="panel_ghost_studio" filename="panel_ghost_studio.xml"/>
static LLPanelInjector<ALPanelGhostStudio> t_panel_ghost_studio("panel_ghost_studio");

namespace
{
// style id -> short list label (keep in sync with EGhostStyle /
// panel_path_editor.xml's ghost_style_combo)
const char* style_name(S32 style)
{
    switch (style)
    {
    case 1:  return "Clone";
    case 2:  return "Hologram";
    case 3:  return "Wireframe";
    case 4:  return "X-ray";
    case 5:  return "Thermal";
    case 6:  return "Neon Outline";
    case 7:  return "Silhouette";
    case 8:  return "Toon/Ink";
    case 9:  return "Chrome";
    case 10: return "Dissolve";
    case 11: return "Negative";
    case 12: return "Gold Statue";
    case 13: return "Night-vision";
    case 14: return "Blueprint";
    case 15: return "Ectoplasm";
    case 16: return "Frost/Ice";
    case 17: return "Prism";
    case 18: return "Thermal Scope";
    case 19: return "Wallhack/ESP";
    case 20: return "NV Tube";
    case 21: return "Damage";
    case 22: return "Killcam";
    case 23: return "Oil Slick";
    case 24: return "Vaporwave";
    case 25: return "Halftone";
    case 26: return "Sonar";
    case 27: return "Holo Echo";
    default: return "Ghost";
    }
}

const char* kind_name(ALGhostStudio::EBackingKind kind)
{
    return kind == ALGhostStudio::BACKING_ENTITY_CLONE ? "Entity" : "Overlay";
}

// [GhostPanelPerf] Debug switch for stale-UI reports: restores the old every-frame
// refresh behaviour (signature compares, ungated detail/crowd blocks, 0.5 s rig
// diagnostics with live columns). Cached control: no settings-map lookup per frame.
bool ghost_refresh_always()
{
    static LLCachedControl<bool> refresh_always(
        gSavedSettings, "GhostStudioRefreshAlways", false);
    // GhostStudioRefreshAlways is an ALIAS of the global UIRefreshForceAlways (either enables it)
    return refresh_always || alUiRefreshForceAlways();
}

const char* state_name(const ALGhostStudio::Instance& inst)
{
    switch (inst.mState)
    {
    case ALGhostStudio::STATE_SPAWNING:       return "spawning";
    case ALGhostStudio::STATE_SOURCE_MISSING: return "source-missing";
    case ALGhostStudio::STATE_LOCKED:         return "locked";
    case ALGhostStudio::STATE_TEARING_DOWN:   return "teardown";
    case ALGhostStudio::STATE_ERROR:           return "error";
    case ALGhostStudio::STATE_RECOVERABLE:     return "recoverable";
    default:
        if (inst.mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            inst.mDriveMode == ALGhostStudio::DRIVE_FROZEN)
        {
            return "frozen";
        }
        return inst.mPose == ALGhostStudio::POSE_FROZEN ? "frozen" : "live";
    }
}

enum class ECrowdParameter
{
    NONE,
    GRID_COLUMNS,
    ARC_SWEEP,
    LANE_GAP,
    CHEVRON_ANGLE,
    ZIGZAG_COLUMNS,
    HORSESHOE_OPENING,
    SPIRAL_TURNS,
    ASPECT_RATIO,
    RAY_COUNT,
    STAGGER_OFFSET
};

// Formations that fill an explicit ROWS x COLUMNS grid and therefore reuse the
// Brigade rank/file spinners for their count, files, ranks and spacing rather
// than the single centre-spacing/count pair.
bool formation_uses_grid(ALGhostStudio::EFormation formation)
{
    return formation == ALGhostStudio::FORMATION_BRIGADE ||
           formation == ALGhostStudio::FORMATION_SOUL_TRAIN_MILITARY ||
           formation == ALGhostStudio::FORMATION_STAGGERED_MILITARY ||
           formation == ALGhostStudio::FORMATION_RING_BRIGADE;
}

struct CrowdParameterDescriptor
{
    ECrowdParameter mParameter = ECrowdParameter::NONE;
    const char* mLabel = "";
    const char* mTooltip = "";
    F32 mMinimum = 0.f;
    F32 mMaximum = 1.f;
    F32 mIncrement = 1.f;
    F32 mDefault = 0.f;
    S32 mPrecision = 0;

    CrowdParameterDescriptor() = default;
    CrowdParameterDescriptor(ECrowdParameter parameter,
                             const char* label,
                             const char* tooltip,
                             F32 minimum,
                             F32 maximum,
                             F32 increment,
                             F32 default_value,
                             S32 precision)
        : mParameter(parameter),
          mLabel(label),
          mTooltip(tooltip),
          mMinimum(minimum),
          mMaximum(maximum),
          mIncrement(increment),
          mDefault(default_value),
          mPrecision(precision)
    {
    }
};

CrowdParameterDescriptor crowd_parameter_descriptor(
    ALGhostStudio::EFormation formation)
{
    using Studio = ALGhostStudio;
    switch (formation)
    {
    case Studio::FORMATION_GRID:
    case Studio::FORMATION_STAGGERED_ROWS:
        return { ECrowdParameter::GRID_COLUMNS, "Columns",
                 "Members per row; 0 chooses a compact layout automatically",
                 0.f, 256.f, 1.f, 0.f, 0 };
    case Studio::FORMATION_ARC:
        return { ECrowdParameter::ARC_SWEEP, "Sweep",
                 "Arc sweep in degrees", 1.f, 360.f, 5.f, 120.f, 1 };
    case Studio::FORMATION_SPLIT_ROW:
    case Studio::FORMATION_SOUL_TRAIN:
        return { ECrowdParameter::LANE_GAP, "Aisle gap",
                 "Clear distance between the two facing lines in metres",
                 0.f, 100.f, 0.1f, 2.f, 2 };
    case Studio::FORMATION_CHEVRON:
        return { ECrowdParameter::CHEVRON_ANGLE, "Angle",
                 "Interior chevron angle in degrees",
                 10.f, 170.f, 5.f, 90.f, 1 };
    case Studio::FORMATION_ZIGZAG:
        return { ECrowdParameter::ZIGZAG_COLUMNS, "Columns",
                 "Number of columns before the zigzag turns",
                 2.f, 256.f, 1.f, 4.f, 0 };
    case Studio::FORMATION_HORSESHOE:
        return { ECrowdParameter::HORSESHOE_OPENING, "Opening",
                 "Open portion of the horseshoe in degrees",
                 1.f, 300.f, 5.f, 90.f, 1 };
    case Studio::FORMATION_SPIRAL:
        return { ECrowdParameter::SPIRAL_TURNS, "Turns",
                 "Number of turns from the centre to the outside",
                 0.25f, 20.f, 0.25f, 2.f, 2 };
    case Studio::FORMATION_INFINITY:
    case Studio::FORMATION_PENTAGRAM:
    case Studio::FORMATION_STAR_OUTLINE:
    case Studio::FORMATION_DIAMOND:
    case Studio::FORMATION_ARROW:
    case Studio::FORMATION_HEART:
        return { ECrowdParameter::ASPECT_RATIO, "Aspect",
                 "Height divided by width (Y/X scale)",
                 0.25f, 4.f, 0.05f, 1.f, 2 };
    case Studio::FORMATION_SUNBURST:
        return { ECrowdParameter::RAY_COUNT, "Rays",
                 "Number of rays around the centre", 3.f, 64.f, 1.f, 8.f, 0 };
    case Studio::FORMATION_SOUL_TRAIN_MILITARY:
        return { ECrowdParameter::LANE_GAP, "Aisle gap",
                 "Clear distance between the two facing blocks in metres",
                 0.f, 100.f, 0.1f, 2.f, 2 };
    case Studio::FORMATION_STAGGERED_MILITARY:
        return { ECrowdParameter::STAGGER_OFFSET, "Row offset",
                 "Alternate-rank shift as a fraction of file spacing; "
                 "positive staggers a brick, negative steps an echelon",
                 -2.f, 2.f, 0.05f, 0.5f, 2 };
    case Studio::FORMATION_RING_BRIGADE:
        return { ECrowdParameter::STAGGER_OFFSET, "Ring offset",
                 "Rotation of alternate rings as a fraction of the member "
                 "angle; 0 lines rings into spokes, 0.5 interleaves them, "
                 "negative twists each ring further (echelon)",
                 -2.f, 2.f, 0.05f, 0.5f, 2 };
    default:
        return {};
    }
}
} // anonymous namespace

ALPanelGhostStudio::ALPanelGhostStudio()
{
    mPlacementOwner.generate();
}

ALPanelGhostStudio::~ALPanelGhostStudio()
{
    // If owner-scoped cancellation invokes the stored callback while this
    // derived destructor is running, make it a no-op before touching the tool.
    mCrowdFacingPrototype.setNull();
    exitPlaceMode();
    exitEditMode();
    releaseOwnedCrowdPlacement();
}

// ---------------------------------------------------------------------------
bool ALPanelGhostStudio::postBuild()
{
    mShowAllCheck  = getChild<LLCheckBoxCtrl>("show_all_check");
    mEditModeCheck = getChild<LLCheckBoxCtrl>("edit_ghosts_check");
    mHint          = getChild<LLTextBox>("studio_hint");
    mList         = getChild<LLScrollListCtrl>("ghost_list");
    mSourceCombo  = getChild<LLComboBox>("source_combo");
    mCloneTypeCombo = getChild<LLComboBox>("clone_type_combo");
    mAddBtn       = getChild<LLButton>("btn_ghost_add");
    mDupBtn       = getChild<LLButton>("btn_ghost_dup");
    mDelBtn       = getChild<LLButton>("btn_ghost_del");
    mRefreshBtn   = getChild<LLButton>("btn_ghost_refresh");
    mTypeText     = getChild<LLTextBox>("ghost_type");
    mNameEdit     = getChild<LLLineEditor>("ghost_name");

    mPosX      = getChild<LLSpinCtrl>("pos_x_spinner");
    mPosY      = getChild<LLSpinCtrl>("pos_y_spinner");
    mPosZ      = getChild<LLSpinCtrl>("pos_z_spinner");
    mHeadingDial = getChild<ALCompassDial>("heading_dial");
    mYawSpin   = getChild<LLSpinCtrl>("yaw_spinner");
    mScaleSpin = getChild<LLSpinCtrl>("scale_spinner");
    mChaosCheck = getChild<LLCheckBoxCtrl>("chaos_check");
    mChaosSlider = getChild<LLSliderCtrl>("chaos_slider");
    mAnimSpeedSpin = getChild<LLSpinCtrl>("anim_speed_spinner");
    // [AutoAnimate]
    mAutoAnimCheck = getChild<LLCheckBoxCtrl>("autoanim_check");
    mAutoAnimWindow = getChild<LLSpinCtrl>("autoanim_window_spinner");
    mAutoAnimStatus = getChild<LLTextBox>("autoanim_status");
    mAutoAnimFollow = getChild<LLButton>("btn_autoanim_follow");
    mAutoAnimGo = getChild<LLButton>("btn_autoanim_go");
    mAutoAnimLoad = getChild<LLButton>("btn_autoanim_load");
    mAutoAnimRelearn = getChild<LLButton>("btn_autoanim_relearn");
    mAutoAnimReseed = getChild<LLButton>("btn_autoanim_reseed");
    mAutoAnimReset = getChild<LLButton>("reset_autoanim_window");
    mAutoAnimSave = getChild<LLButton>("btn_autoanim_save");
    mAutoAnimStop = getChild<LLButton>("btn_autoanim_stop");
    mAutoAnimGo->setToolTip(std::string("Keep the current performance. True mirror switches to Mirror with a COLD body start (not seamless)."));
    mPhysicsCheck = getChild<LLCheckBoxCtrl>("physics_check");
    mAnimPauseBtn = getChild<LLButton>("btn_anim_pause");
    mAnimResumeBtn = getChild<LLButton>("btn_anim_resume");
    mDriveModeCombo = getChild<LLComboBox>("drive_mode_combo");
    mDirectedAnimEdit = getChild<LLLineEditor>("directed_anim_editor");
    mAnimationLibraryCombo = getChild<LLComboBox>("animation_library_combo");
    mAnimationLibraryRefreshBtn = getChild<LLButton>("btn_anim_library_refresh");
    mLoopModeCombo = getChild<LLComboBox>("anim_loop_mode_combo");
    mAnimSyncBtn = getChild<LLButton>("btn_anim_sync");
    mLensGazeCheck = getChild<LLCheckBoxCtrl>("lens_gaze_check");
    mLensGazeSelectionBtn = getChild<LLButton>("btn_lens_gaze_selection");
    mLensGazeTorsoSlider = getChild<LLSliderCtrl>("lens_gaze_torso");
    mLensGazeTargetMode = getChild<LLComboBox>("lens_gaze_target_mode");
    mLensGazeCastTarget = getChild<LLComboBox>("lens_gaze_cast_target");
    mLensGazeHeadEyeSlider = getChild<LLSliderCtrl>("lens_gaze_head_eye");
    mLensGazeIntensitySlider = getChild<LLSliderCtrl>("lens_gaze_intensity");
    mLensGazeSmoothingSlider = getChild<LLSliderCtrl>("lens_gaze_smoothing");
    mLensGazeStatus = getChild<LLTextBox>("lens_gaze_status");
    mAnimMetadataText = getChild<LLTextBox>("anim_metadata_text");
    mLookSection = getChild<LLView>("look_section");
    mEntityStyleHint = getChild<LLTextBox>("entity_style_hint");
    mPlaceBtn  = getChild<LLButton>("btn_place");
    mToActorBtn = getChild<LLButton>("btn_to_actor");
    mToMeBtn   = getChild<LLButton>("btn_to_me");
    mDropBtn = getChild<LLButton>("btn_drop_ground");
    mAlignFeetBtn = getChild<LLButton>("btn_align_feet");
    mUprightBtn = getChild<LLButton>("btn_reset_upright");
    mDupInPlaceBtn = getChild<LLButton>("btn_duplicate_in_place");
    mCopyTransformBtn = getChild<LLButton>("btn_copy_transform");
    mPasteTransformBtn = getChild<LLButton>("btn_paste_transform");
    mLookTargetCombo = getChild<LLComboBox>("look_target_combo");
    mFaceNowBtn = getChild<LLButton>("btn_face_now");
    mKeepFacingCheck = getChild<LLCheckBoxCtrl>("keep_facing_check");

    mStyleCombo     = getChild<LLComboBox>("style_combo");
    mActorTintCheck = getChild<LLCheckBoxCtrl>("actor_tint_check");
    mHueSlider      = getChild<LLSliderCtrl>("hue_slider");
    mAlphaSlider    = getChild<LLSliderCtrl>("alpha_slider");
    mShimmerSpeedSlider  = getChild<LLSliderCtrl>("shimmer_speed_slider");
    mShimmerAmountSlider = getChild<LLSliderCtrl>("shimmer_amount_slider");
    mPixelSlider    = getChild<LLSliderCtrl>("pixel_slider");
    mGlitchSlider   = getChild<LLSliderCtrl>("glitch_slider");
    mDistortCombo   = getChild<LLComboBox>("distort_combo");
    mDistortAmountSlider = getChild<LLSliderCtrl>("distort_amount_slider");
    mBrightnessSlider = getChild<LLSliderCtrl>("brightness_slider");
    mEffectFps = getChild<LLSpinCtrl>("effect_fps_spinner");
    mEntityLookCombo = getChild<LLComboBox>("entity_look_combo");

    mFreezeBtn  = getChild<LLButton>("btn_freeze");
    mLiveBtn    = getChild<LLButton>("btn_live");
    mPoseStatus = getChild<LLTextBox>("pose_status");
    mRigDiagnostics = getChild<LLTextEditor>("rig_diagnostics");
    mRigDiagRefreshBtn = getChild<LLButton>("btn_rig_diag_refresh"); // [GhostPanelPerf]

    mArrayCount   = getChild<LLSpinCtrl>("array_count_spinner");
    mArraySpacing = getChild<LLSpinCtrl>("array_spacing_spinner");
    mArrayLineBtn = getChild<LLButton>("btn_array_line");
    mArrayRingBtn = getChild<LLButton>("btn_array_ring");
    mFormationCombo = getChild<LLComboBox>("array_formation_combo");
    mFormationParam = getChild<LLSpinCtrl>("array_parameter_spinner");
    mFormationParamReset = getChild<LLButton>("reset_array_parameter");
    mFormationSeed = getChild<LLSpinCtrl>("formation_seed_spinner");
    mFormationJitter = getChild<LLSpinCtrl>("formation_jitter_spinner");
    mFormationGuideShape =
        getChild<LLComboBox>("formation_guide_shape_combo");
    mFormationGuideSize =
        getChild<LLSpinCtrl>("formation_guide_size_spinner");
    mFormationEdgeGap = getChild<LLSpinCtrl>("formation_edge_gap_spinner");
    mBrigadeColumns = getChild<LLSpinCtrl>("brigade_columns_spinner");
    mBrigadeRows = getChild<LLSpinCtrl>("brigade_rows_spinner");
    mBrigadeFileSpacing =
        getChild<LLSpinCtrl>("brigade_file_spacing_spinner");
    mBrigadeRankSpacing =
        getChild<LLSpinCtrl>("brigade_rank_spacing_spinner");
    mFormationFacing = getChild<LLComboBox>("formation_facing_combo");
    mFormationTerrain = getChild<LLCheckBoxCtrl>("formation_terrain_check");
    mFacingYawOffset = getChild<LLSpinCtrl>("facing_yaw_offset_spinner");
    mPreserveSourceTilt =
        getChild<LLCheckBoxCtrl>("preserve_source_tilt_check");
    mCrowdCopiesAs = getChild<LLComboBox>("crowd_copies_as_combo");
    mCrowdCreateMode = getChild<LLComboBox>("crowd_create_mode_combo");
    mLockMode = getChild<LLComboBox>("lock_mode_combo");
    mPickFacingTarget = getChild<LLButton>("btn_pick_facing_target");
    mArrayBuildBtn = getChild<LLButton>("btn_array_build");
    mCrowdConfirmBtn = getChild<LLButton>("btn_crowd_confirm");
    mCrowdUnpinBtn = getChild<LLButton>("btn_crowd_unpin");
    mCrowdCancelBtn = getChild<LLButton>("btn_crowd_cancel");
    mCrowdPlacementStatus = getChild<LLTextBox>("crowd_placement_status");
    mCrowdInstructions = getChild<LLTextBox>("crowd_placement_instructions");
    mNameplateMode = getChild<LLComboBox>("nameplate_mode_combo");
    mGroupEditMembersCheck =
        getChild<LLCheckBoxCtrl>("group_edit_members_check");
    mGroupMemberPinnedCheck =
        getChild<LLCheckBoxCtrl>("group_member_pinned_check");
    mGroupMemberResetBtn = getChild<LLButton>("btn_group_member_reset");
    mStripCount = getChild<LLSpinCtrl>("strip_count_spinner");
    mStripInterval = getChild<LLSpinCtrl>("strip_interval_spinner");
    mStripStartBtn = getChild<LLButton>("btn_strip_start");
    mStripCancelBtn = getChild<LLButton>("btn_strip_cancel");
    mStripStatus = getChild<LLTextBox>("strip_status");
    mMotionCombo = getChild<LLComboBox>("formation_motion_combo");
    mMotionSpeed = getChild<LLSpinCtrl>("formation_motion_speed");
    mMotionAmplitude = getChild<LLSpinCtrl>("formation_motion_amplitude");
    mMotionApplyBtn = getChild<LLButton>("btn_formation_motion_apply");
    mGroupBtn = getChild<LLButton>("btn_group_unit");
    mUngroupBtn = getChild<LLButton>("btn_ungroup_unit");
    mPoseRate = getChild<LLSpinCtrl>("pose_rate_spinner");

    mStatusText = getChild<LLTextBox>("studio_status");

    // XUI groups each tab beside the section it owns for maintainability.
    // Reinsert the last three panels in the requested visible order.
    LLTabContainer* tabs = getChild<LLTabContainer>("ghost_studio_tabs");
    LLPanel* pose_tab = getChild<LLPanel>("pose_tab");
    LLPanel* style_tab = getChild<LLPanel>("style_tab");
    LLPanel* crowd_tab = getChild<LLPanel>("crowd_tab");
    tabs->removeTabPanel(style_tab);
    tabs->removeTabPanel(pose_tab);
    tabs->removeTabPanel(crowd_tab);
    tabs->addTabPanel(pose_tab);
    tabs->addTabPanel(style_tab);
    tabs->addTabPanel(crowd_tab);

    // The shared panel is hosted both standalone and in Director. Keep Tab
    // focus visible in either viewport without giving the host another wheel
    // owner around these tab-local documents.
    ALScrollFocus::install(this, "place_scroll", "place_scroll_body");
    ALScrollFocus::install(this, "style_scroll", "style_scroll_body");
    ALScrollFocus::install(this, "pose_scroll", "pose_scroll_body");
    ALScrollFocus::install(this, "crowd_scroll", "crowd_scroll_body");

    mShowAllCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShowAllToggle(); });
    mEditModeCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onToggleEditMode(); });
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onListSelect(); });
    mList->setMouseUpCallback(
        [this](LLUICtrl*, S32 x, S32 y, MASK mask) { onListMouseUp(x, y, mask); });
    mList->setDoubleClickCallback([this]() { onListDoubleClick(); });
    mAddBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAdd(); });
    mNameEdit->setCommitCallback([this](LLUICtrl*, const LLSD&) { onNameCommit(); });
    mDupBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDuplicate(); });
    mDelBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDelete(); });
    mRefreshBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickRefresh(); });

    mPosX->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    mPosY->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    mPosZ->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    // ALCompassDial commits on mouse-down and every hover while captured, so
    // this is deliberately the live-drag callback (same idiom as Actor Mover).
    mHeadingDial->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onHeadingDialCommit(); });
    mYawSpin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onYawCommit(); });
    mScaleSpin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScaleCommit(); });
    mChaosCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onChaosCommit(); });
    mChaosSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onChaosCommit(); });
    mAnimSpeedSpin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimSpeedCommit(); });
    // [AutoAnimate] New handlers expand selected groups just like drive-mode edits.
    mAutoAnimCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimToggle(); });
    mAutoAnimWindow->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimWindow(); });
    mAutoAnimStop->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimAction(ALGhostAutoAnim::Action::StopLearning); });
    mAutoAnimRelearn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimAction(ALGhostAutoAnim::Action::Relearn); });
    mAutoAnimReseed->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimAction(ALGhostAutoAnim::Action::Reseed); });
    mAutoAnimGo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimAction(ALGhostAutoAnim::Action::GoAutonomous); });
    mAutoAnimFollow->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimAction(ALGhostAutoAnim::Action::FollowSource); });
    mAutoAnimSave->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimFile(true); });
    mAutoAnimLoad->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAnimFile(false); });
    mPhysicsCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPhysicsCommit(); });
    mAnimPauseBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAnimPause(); });
    mAnimResumeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAnimResume(); });
    mDriveModeCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDriveModeCommit(); });
    mDirectedAnimEdit->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDirectedAnimCommit(); });
    mAnimationLibraryCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimationLibraryCommit(); });
    mAnimationLibraryRefreshBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            // explicit re-scan (a click, not a draw): shared cache, then mirror it
            ALGhostStudio::instance().scanAnimLibrary();
            populateAnimationLibrary();
        });
    mLoopModeCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onLoopModeCommit(); });
    mAnimSyncBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAnimSync(); });
    mLensGazeCheck->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLensGazeToggle(); });
    mLensGazeSelectionBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickLensGazeSelection(); });
    mLensGazeTorsoSlider->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLensGazeTorsoCommit(); });
    mLensGazeTargetMode->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLensGazeSettingsCommit(); });
    mLensGazeCastTarget->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLensGazeSettingsCommit(); });
    mLensGazeHeadEyeSlider->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLensGazeSettingsCommit(); });
    mLensGazeIntensitySlider->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLensGazeSettingsCommit(); });
    mLensGazeSmoothingSlider->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLensGazeSettingsCommit(); });
    mLensGazeTargetMode->setValue(LLActorMover::GAZE_CAMERA);
    populateLensGazeCastTargets();
    mPlaceBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickPlace(); });
    mToActorBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickToActor(); });
    mToMeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickToMe(); });
    mDropBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDrop(); });
    mAlignFeetBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAlignFeet(); });
    mUprightBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickUpright(); });
    mDupInPlaceBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDuplicateInPlace(); });
    mCopyTransformBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickCopyTransform(); });
    mPasteTransformBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickPasteTransform(); });
    mLookTargetCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLookTargetCommit(); });
    mFaceNowBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickFaceNow(); });
    mKeepFacingCheck->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onKeepFacingCommit(); });

    mStyleCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStyleCommit(); });
    mActorTintCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorTintToggle(); });
    mHueSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onHueCommit(); });
    mAlphaSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAlphaCommit(); });
    mShimmerSpeedSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShimmerSpeedCommit(); });
    mShimmerAmountSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShimmerAmountCommit(); });
    mPixelSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPixelCommit(); });
    mGlitchSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGlitchCommit(); });
    mDistortCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDistortCommit(); });
    mDistortAmountSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDistortAmountCommit(); });
    mBrightnessSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBrightnessCommit(); });
    mEffectFps->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEffectFpsCommit(); });
    mEntityLookCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEntityLookCommit(); });

    mFreezeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickFreeze(); });
    mLiveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickLive(); });
    mRigDiagRefreshBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickRigDiagRefresh(); }); // [GhostPanelPerf]

    mArrayLineBtn->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { onClickArray(ALGhostStudio::FORMATION_LINE); });
    mArrayRingBtn->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { onClickArray(ALGhostStudio::FORMATION_RING); });
    mFormationCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onFormationCommit(); });
    mFormationParam->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        captureFormationParameter();
        updateOwnedCrowdPlacement();
    });
    mFormationParamReset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            const CrowdParameterDescriptor descriptor =
                crowd_parameter_descriptor(mConfiguredFormation);
            if (descriptor.mParameter == ECrowdParameter::NONE)
            {
                return;
            }
            mFormationParam->setValue(descriptor.mDefault);
            captureFormationParameter();
            updateOwnedCrowdPlacement();
        });
    for (LLUICtrl* control :
         { static_cast<LLUICtrl*>(mArrayCount),
           static_cast<LLUICtrl*>(mArraySpacing),
           static_cast<LLUICtrl*>(mFormationSeed),
           static_cast<LLUICtrl*>(mFormationJitter),
           static_cast<LLUICtrl*>(mFormationGuideShape),
           static_cast<LLUICtrl*>(mFormationGuideSize),
           static_cast<LLUICtrl*>(mFormationEdgeGap),
           static_cast<LLUICtrl*>(mBrigadeColumns),
           static_cast<LLUICtrl*>(mBrigadeRows),
           static_cast<LLUICtrl*>(mBrigadeFileSpacing),
           static_cast<LLUICtrl*>(mBrigadeRankSpacing),
           static_cast<LLUICtrl*>(mFormationFacing),
           static_cast<LLUICtrl*>(mFormationTerrain),
           static_cast<LLUICtrl*>(mPreserveSourceTilt),
           static_cast<LLUICtrl*>(mCrowdCreateMode) })
    {
        control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { updateOwnedCrowdPlacement(); });
    }
    // Independent of FormationSpec: the facing-offset bias lives directly on
    // the crowd draft (CrowdPlacementDraft::mFacingYawOffset), not the spec,
    // so it goes through its own dedicated setter rather than
    // updateOwnedCrowdPlacement()'s spec re-resolve.
    mFacingYawOffset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            const ALGhostStudio::CrowdPlacementDraft& draft =
                ALGhostStudio::instance().getCrowdPlacementDraft();
            if (!draft.mActive || draft.mOwnerId != mPlacementOwner)
            {
                return;
            }
            const F32 offset_rad =
                (F32)mFacingYawOffset->getValue().asReal() * DEG_TO_RAD;
            ALGhostStudio::instance().setCrowdPlacementFacingOffset(
                mPlacementOwner, offset_rad);
            refreshCrowdPlacement();
        });
    mArrayBuildBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickBuildArray(); });
    mCrowdConfirmBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickCrowdConfirm(); });
    mCrowdUnpinBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickCrowdUnpin(); });
    mCrowdCancelBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickCrowdCancel(); });
    mStripStartBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickStartStrip(); });
    mStripCancelBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickCancelStrip(); });
    mMotionApplyBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onMotionCommit(); });
    mPickFacingTarget->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickPickFacingTarget(); });
    mGroupBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroup(); });
    mUngroupBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickUngroup(); });
    mLockMode->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        const ALGhostStudio::CrowdPlacementDraft& draft =
            ALGhostStudio::instance().getCrowdPlacementDraft();
        if (!draft.mActive)
        {
            onLockModeCommit();
        }
        updateOwnedCrowdPlacement();
    });
    mGroupEditMembersCheck->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGroupEditMembersCommit(); });
    mGroupMemberPinnedCheck->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGroupMemberPinnedCommit(); });
    mGroupMemberResetBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickGroupMemberReset(); });
    mNameplateMode->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onNameplateModeCommit(); });
    mPoseRate->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPoseRateCommit(); });

    // Every reset reads the target's XUI initial_value and invokes that
    // target's existing commit callback. Future numeric controls need only
    // one table row here and one XUI button.
    static const std::pair<const char*, const char*> reset_controls[] =
    {
        { "reset_yaw", "yaw_spinner" },
        { "reset_scale", "scale_spinner" },
        { "reset_effect_fps", "effect_fps_spinner" },
        { "reset_anim_speed", "anim_speed_spinner" },
        { "reset_autoanim_window", "autoanim_window_spinner" }, // [AutoAnimate]
        { "reset_array_count", "array_count_spinner" },
        { "reset_array_spacing", "array_spacing_spinner" },
        { "reset_strip_count", "strip_count_spinner" },
        { "reset_strip_interval", "strip_interval_spinner" },
        { "reset_motion_speed", "formation_motion_speed" },
        { "reset_motion_amplitude", "formation_motion_amplitude" },
        { "reset_hue", "hue_slider" },
        { "reset_alpha", "alpha_slider" },
        { "reset_pixel", "pixel_slider" },
        { "reset_shimmer_speed", "shimmer_speed_slider" },
        { "reset_shimmer_amount", "shimmer_amount_slider" },
        { "reset_glitch", "glitch_slider" },
        { "reset_distort_amount", "distort_amount_slider" },
        { "reset_brightness", "brightness_slider" },
        { "reset_chaos", "chaos_slider" },
        { "reset_lens_gaze_torso", "lens_gaze_torso" },
        { "reset_lens_gaze_head_eye", "lens_gaze_head_eye" },
        { "reset_lens_gaze_intensity", "lens_gaze_intensity" },
        { "reset_lens_gaze_smoothing", "lens_gaze_smoothing" }
    };
    for (const auto& entry : reset_controls)
    {
        const std::string target_name(entry.second);
        getChild<LLButton>(entry.first)->setCommitCallback(
            [this, target_name](LLUICtrl*, const LLSD&) { onResetNumeric(target_name); });
    }
    onFormationCommit();

    mShowAllCheck->set(ALGhostStudio::instance().getShowAll());
    mNameplateMode->setValue(gSavedSettings.getS32("GhostStudioNameplateMode"));
    return true;
}

void ALPanelGhostStudio::onVisibilityChange(bool new_visibility)
{
    if (!new_visibility)
    {
        // never strand the user in a tool when the panel hides
        exitPlaceMode();
        exitEditMode();
        releaseOwnedCrowdPlacement();
    }
    else
    {
        // [GhostPanelPerf] The first frame after a re-show is a FULL refresh, so
        // correctness never depends on the gates having seen every revision while
        // hidden (the host may have been a hidden Director tab, or scrolled away).
        mListGate.reset();
        mSourceGate.reset();
        mLookTargetGate.reset();
        mDetailGate.reset();
        mCrowdGate.reset();
        mReadoutClock.forceDue();
        mStatusClock.forceDue();
        mDetailDirty = true;
        mCrowdDirty = true;
        mRigDiagShown = false;
        noteListSelectionChanged();
        mAnimLibraryShownRev = 0; // re-mirror the shared library when directed controls show
    }
    LLPanel::onVisibilityChange(new_visibility);
}

// [GhostPanelPerf] Cached selection snapshot. Rebuilt only when the list selection
// (epoch, bumped at every programmatic change and by onListSelect), the shared
// selection, or the instance model moved; num-selected / first-index are an
// allocation-free safety net for a missed epoch bump.
const ALPanelGhostStudio::SelectionCache& ALPanelGhostStudio::selection()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const U64 instance_rev = studio.getInstanceRevision();
    const U64 shared_rev = studio.getSelectionRevision();
    const S32 num_selected = mList->getNumSelected();
    const S32 first_index = mList->getFirstSelectedIndex();
    SelectionCache& cache = mSel;
    // The gate revalidates on any revision / list-state change AND, via its built-in
    // 1 s fallback, at least once a second even if a bump was missed.
    if (!mSelectionGate.changed({ mSelectionEpoch, instance_rev, shared_rev,
                                  (U64)(S64)num_selected, (U64)(S64)first_index }))
    {
        return cache;
    }
    cache.mEpoch = mSelectionEpoch;
    cache.mInstanceRev = instance_rev;
    cache.mSharedRev = shared_rev;
    cache.mNumSelected = num_selected;
    cache.mFirstIndex = first_index;
    ++cache.mStamp;
    cache.mRow = selectedListValue();
    cache.mInstance = selectedInstance();
    cache.mInstances = selectedInstances();
    cache.mAnyGroup = false;
    cache.mAnyEntity = false;
    for (const LLUUID& id : cache.mInstances)
    {
        if (studio.groupForMember(id))
        {
            cache.mAnyGroup = true;
        }
        const ALGhostStudio::Instance* selected = studio.getInstance(id);
        if (selected && selected->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            selected->mEntityId.notNull())
        {
            cache.mAnyEntity = true;
        }
    }
    return cache;
}

// ---------------------------------------------------------------------------
std::string ALPanelGhostStudio::sourceName(const LLUUID& id)
{
    if (id.isNull())
    {
        return std::string("You");
    }
    if (const LLDirectorCast::CastMember* m = LLDirectorCast::instance().getMember(id))
    {
        if (!m->mLastName.empty())
        {
            return m->mLastName;
        }
    }
    if (LLGhostAvatar::isGhostId(id))
    {
        return std::string("Entity clone ") + id.asString().substr(0, 8);
    }
    if (LLAvatarName av_name; LLAvatarNameCache::get(id, &av_name))
    {
        return av_name.getCompleteName();
    }
    return id.asString().substr(0, 8);
}

LLUUID ALPanelGhostStudio::selectedInstance() const
{
    const LLUUID value = selectedListValue();
    if (const ALGhostGroupModel::Group* group =
            ALGhostStudio::instance().groupForMember(value);
        group && value == group->mId && !group->mMembers.empty())
    {
        return group->mMembers.front().mInstanceId;
    }
    return value;
}

LLUUID ALPanelGhostStudio::selectedListValue() const
{
    LLScrollListItem* item = mList->getFirstSelected();
    return item ? item->getValue().asUUID() : LLUUID::null;
}

LLUUID ALPanelGhostStudio::selectedUnitId() const
{
    const LLUUID row_value = selectedListValue();
    const ALGhostGroupModel::Group* group =
        ALGhostStudio::instance().groupForMember(row_value);
    return group && row_value == group->mId
        ? group->mId : selectedInstance();
}

std::vector<LLUUID> ALPanelGhostStudio::selectedInstances() const
{
    std::vector<LLUUID> ids;
    std::set<LLUUID> unique;
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID value = item->getValue().asUUID();
        const ALGhostGroupModel::Group* group =
            ALGhostStudio::instance().groupForMember(value);
        if (group && value == group->mId)
        {
            for (const ALGhostGroupModel::Member& member : group->mMembers)
            {
                if (unique.insert(member.mInstanceId).second)
                    ids.push_back(member.mInstanceId);
            }
        }
        else if (unique.insert(value).second)
        {
            ids.push_back(value);
        }
    }
    return ids;
}

std::vector<LLUUID> ALPanelGhostStudio::selectedUnitIds() const
{
    std::vector<LLUUID> ids;
    std::set<LLUUID> unique;
    std::set<LLUUID> selected_group_headers;
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID row_value = item->getValue().asUUID();
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(row_value);
        if (group && row_value == group->mId)
        {
            selected_group_headers.insert(group->mId);
        }
    }
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID row_value = item->getValue().asUUID();
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(row_value);
        if (group && row_value != group->mId &&
            selected_group_headers.find(group->mId) !=
                selected_group_headers.end())
        {
            // A selected header already owns this authoring unit. Applying the
            // same command to one of its selected child rows as well would move
            // the group and then add an unintended member-local edit.
            continue;
        }
        const LLUUID unit_id =
            group && row_value == group->mId
                ? group->mId : row_value;
        if (unit_id.notNull() && unique.insert(unit_id).second)
        {
            ids.push_back(unit_id);
        }
    }
    return ids;
}

bool ALPanelGhostStudio::selectedUnitTransform(
    LLVector3d& foot, LLQuaternion& rotation, F32& scale,
    bool& collapsed_group) const
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row_value = selectedListValue();
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(row_value);
    // A group header always represents the group transform, even while its
    // member rows are expanded. Only an explicit child row edits one member.
    collapsed_group = group && row_value == group->mId;
    const LLUUID id = collapsed_group ? group->mId : selectedInstance();
    return id.notNull() &&
           studio.getUnitTransform(id, foot, rotation, scale);
}

bool ALPanelGhostStudio::applySelectedUnitTransform(
    const LLVector3d& foot, const LLQuaternion& rotation, F32 scale)
{
    return applyUnitTransform(
        selectedUnitId(), foot, rotation, scale);
}

bool ALPanelGhostStudio::applyUnitTransform(
    const LLUUID& unit_id, const LLVector3d& foot,
    const LLQuaternion& rotation, F32 scale)
{
    if (unit_id.isNull())
    {
        return false;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(unit_id);
    return group && unit_id == group->mId
        ? studio.transformGroup(unit_id, foot, rotation, scale)
        : studio.transformUnit(unit_id, foot, rotation, scale);
}

// ---------------------------------------------------------------------------
void ALPanelGhostStudio::draw()
{
    // [GhostPanelPerf] named Tracy zone ("ALPanelGhostStudio::draw") so the next
    // regression shows up under this panel instead of inside LLFloater::draw.
    // draw() only consults revisions / clocks and runs diff-guarded writes; see
    // aluirefresh.h and CLAUDE.md "UI PANELS MUST NOT DO WORK IN draw()".
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    ALGhostStudio& studio = ALGhostStudio::instance();
    // Sample the readout clocks ONCE per draw so every refresher shares the tick.
    mReadoutTick = mReadoutClock.due();
    mStatusTick = mStatusClock.due();

    // keep the master check honest against external state (diff-guarded, forced:
    // a check box that was merely clicked must still follow the model)
    setValueIfChanged(mShowAllCheck, LLSD(studio.getShowAll()), true);

    // [R2-3] if our edit tool was taken away (build tools, another picker, Esc
    // inside the tool), reflect that in the toggle instead of a stale "on" --
    // the path panel's exact idiom
    // Edit mode is the tool's EXPLICIT state, NOT "ALToolGhostEdit is current":
    // once a ghost is selected the proxy hands off to the stock translate/rotate
    // tool, so the ghost tool is no longer current while editing continues.
    ALToolGhostEdit* edit_tool = ALToolGhostEdit::getInstance();
    mEditMode =
        edit_tool->isEditModeActive() &&
        edit_tool->editOwner() == mPlacementOwner;
    setValueIfChanged(mEditModeCheck, LLSD(mEditMode), true);
    // hint line follows the mode (diffed; tells the hands what they can do)
    if (mHintEdit != mEditMode)
    {
        mHintEdit = mEditMode;
        setTextIfChanged(mHint, mEditMode
            ? std::string("Click a ghost to select \xC2\xB7 move/rotate with the gizmo \xC2\xB7 scale below \xC2\xB7 Del removes \xC2\xB7 Esc exits")
            : std::string("Styled copies of cast bodies: place, pose, multiply. Double-click clones to show/hide; groups to expand"));
    }

    refreshSourceCombo();
    refreshList();
    refreshLookTargetCombo();

    // [R2-3] mirror the SHARED selection (the in-world edit tool writes it;
    // both panel hosts follow). selectByID is programmatic -- no commit loop.
    const LLUUID& shared_sel = studio.getSelected();
    const U64 shared_selection_revision =
        studio.getSelectionRevision();
    LLUUID desired_row = shared_sel;
    if (const ALGhostGroupModel::Group* group =
            studio.groupForMember(shared_sel);
        group && !group->mEditMembers)
    {
        desired_row = group->mId;
    }
    if (shared_selection_revision != mObservedSelectionRevision)
    {
        // The other panel or in-world picker changed the shared primary row.
        // This list is multi-select, so selectByID() alone would retain stale
        // local extras and could leave the detail editor on the wrong row.
        mList->deselectAllItems(true);
        if (desired_row.notNull())
        {
            mList->selectByID(desired_row);
        }
        mObservedSelectionRevision = shared_selection_revision;
        noteListSelectionChanged();
    }
    else
    {
        // [GhostPanelPerf] compare against the cached row value (no LLSD/UUID
        // temporaries per frame); the cache is keyed on the list selection.
        const LLUUID listed_row = selection().mRow;
        if (desired_row != listed_row)
        {
            mList->deselectAllItems(true);
            if (desired_row.notNull())
            {
                mList->selectByID(desired_row);
            }
            // Only invalidate when the list really changed: a desired row that is
            // not in the list must not churn the cache (and the detail gate) per frame.
            if (selectedListValue() != listed_row)
            {
                noteListSelectionChanged();
            }
        }
    }
    syncCrowdFacingPointPrototype();

    refreshDetail();
    refreshRigDiagnostics();
    refreshStatus();
    refreshCrowdPlacement();
    {
        // Cached control, diff-guarded write; focus-aware (never fights an open combo).
        static LLCachedControl<S32> nameplate_mode(
            gSavedSettings, "GhostStudioNameplateMode", 1);
        setValueIfChanged(mNameplateMode, LLSD((S32)nameplate_mode));
    }
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost panel child draw");
        LLPanel::draw();
    }
}

// ---------------------------------------------------------------------------
// refreshers
// ---------------------------------------------------------------------------
// [GhostPanelPerf] Legacy signature builders: used ONLY while GhostStudioRefreshAlways
// is on (the old every-frame string-diff behaviour, for chasing stale-UI reports).
std::string ALPanelGhostStudio::sourceSignature() const
{
    std::string sig;
    for (const LLDirectorCast::CastMember& m : LLDirectorCast::instance().getCast())
    {
        sig += m.mId.asString();
        sig += m.mLastName;
        sig += '|';
    }
    return sig;
}

std::string ALPanelGhostStudio::lookTargetSignature() const
{
    std::string sig;
    for (const LLDirectorCast::CastMember& m : LLDirectorCast::instance().getCast())
    {
        sig += m.mId.asString() + m.mLastName;
    }
    for (const ALGhostStudio::Instance& inst : ALGhostStudio::instance().getInstances())
    {
        sig += inst.mId.asString() + inst.mName;
    }
    return sig;
}

std::string ALPanelGhostStudio::listSignature() const
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::string sig;
    std::set<LLUUID> signed_groups;
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        sig += inst.mId.asString();
        sig += inst.mName;
        sig += inst.mEnabled ? '+' : '-';
        sig += kind_name(inst.mKind);
        sig += state_name(inst);
        sig += style_name(inst.mStyle);
        sig += (inst.mPose == ALGhostStudio::POSE_FROZEN) ? 'F' : 'L';
        sig += sourceName(inst.mSource);
        sig += inst.mGroupId.asString();
        sig += llformat("%d", (S32)inst.mLockMode);
        sig += '|';
        if (const ALGhostGroupModel::Group* group =
                studio.groupForMember(inst.mId);
            group && signed_groups.insert(group->mId).second)
        {
            sig += group->mId.asString();
            sig += group->mName;
            sig += group->mEditMembers ? 'E' : 'C';
            for (const ALGhostGroupModel::Member& member : group->mMembers)
            {
                sig += member.mPinned ? 'P' : '-';
            }
            sig += '|';
        }
    }
    return sig;
}

void ALPanelGhostStudio::refreshSourceCombo()
{
    // "You" plus every cast member; rebuilt only when the cast revision moves
    // (membership or a cached name) so the dropdown never shuffles mid-interaction.
    // The gate is unprimed until the first call, so a fresh login (empty cast) still
    // builds once and adds the default "You" entry.
    LLDirectorCast& cast = LLDirectorCast::instance();
    bool rebuild = mSourceGate.changed({ cast.getCastRevision() }, ghost_refresh_always());
    if (rebuild && mSourceGate.lastWasFallback())
    {
        // the gate's built-in 1 s self-heal (or a debug force): verify by signature
        rebuild = sourceSignature() != mSourceSig;
    }
    if (!rebuild)
    {
        return;
    }
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost rebuild source combo");
    const LLSD prev = mSourceCombo->getSelectedValue();
    mSourceCombo->clearRows();
    mSourceCombo->add("You", LLSD(LLUUID::null));
    for (const LLDirectorCast::CastMember& m : cast.getCast())
    {
        mSourceCombo->add(sourceName(m.mId), LLSD(m.mId));
    }
    if (!prev.isDefined() || !mSourceCombo->setSelectedByValue(prev, true))
    {
        mSourceCombo->selectFirstItem();    // "You"
    }
    populateLensGazeCastTargets();
    mSourceSig = sourceSignature(); // keep the fallback diff baseline current
}

void ALPanelGhostStudio::populateLensGazeCastTargets()
{
    const LLSD previous = mLensGazeCastTarget->getSelectedValue();
    mLensGazeCastTarget->clearRows();
    for (const LLDirectorCast::CastMember& member :
         LLDirectorCast::instance().getCast())
    {
        mLensGazeCastTarget->add(sourceName(member.mId), LLSD(member.mId));
    }
    if (!previous.isDefined() ||
        !mLensGazeCastTarget->setSelectedByValue(previous, true))
    {
        mLensGazeCastTarget->selectFirstItem();
    }
}

void ALPanelGhostStudio::refreshLookTargetCombo()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    bool rebuild = mLookTargetGate.changed(
        { LLDirectorCast::instance().getCastRevision(),
          studio.getInstanceRevision() }, ghost_refresh_always());
    if (rebuild && mLookTargetGate.lastWasFallback())
    {
        rebuild = lookTargetSignature() != mLookTargetSig;
    }
    if (!rebuild)
    {
        return;
    }
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost rebuild look-target combo");
    const LLSD previous = mLookTargetCombo->getSelectedValue();
    mLookTargetCombo->clearRows();
    mLookTargetCombo->add("Camera", "camera");
    mLookTargetCombo->add("Me", "me");
    for (const LLDirectorCast::CastMember& m : LLDirectorCast::instance().getCast())
    {
        mLookTargetCombo->add("Actor: " + sourceName(m.mId),
                              "actor:" + m.mId.asString());
    }
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        mLookTargetCombo->add("Ghost: " + inst.mName,
                              "ghost:" + inst.mId.asString());
    }
    if (!previous.isDefined() ||
        !mLookTargetCombo->setSelectedByValue(previous, true))
    {
        mLookTargetCombo->selectFirstItem();
    }
    mLookTargetSig = lookTargetSignature(); // fallback diff baseline
}

// [GhostPanelPerf] Mirrors the SHARED library (ALGhostStudio::animLibrary, scanned
// lazily outside draw() and cached for both hosts) into this panel's combo. No
// inventory walk happens here; it is revision-gated by mAnimLibraryShownRev.
void ALPanelGhostStudio::populateAnimationLibrary()
{
    // Never clear a combo the operator has focused or open (a scan / Refresh from the
    // OTHER host lands here too): leave the shown revision stale so it retries.
    if (alIsBeingEdited(mAnimationLibraryCombo))
    {
        return;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    mAnimLibraryShownRev = studio.animLibraryRevision();
    const LLSD selected = mAnimationLibraryCombo->getSelectedValue();
    mAnimationLibraryCombo->removeall();
    mAnimationLibraryCombo->add("Choose inventory animation...", LLUUID::null);
    for (const auto& entry : studio.animLibrary())
    {
        mAnimationLibraryCombo->add(entry.first, entry.second);
    }
    if (!selected.isUndefined())
    {
        mAnimationLibraryCombo->setSelectedByValue(selected, true);
    }
}
void ALPanelGhostStudio::refreshList(bool force_now)
{
    ALGhostStudio& studio = ALGhostStudio::instance();

    // [GhostPanelPerf] rebuild rows only when the instance/group model or the cast
    // (source-name column) revision moved -- no signature strings in the draw path.
    const std::initializer_list<U64> revisions =
        { studio.getInstanceRevision(), LLDirectorCast::instance().getCastRevision() };
    bool stale = mListGate.isStale(revisions) || ghost_refresh_always();
    if (stale && (mListGate.lastWasFallback() || ghost_refresh_always()))
    {
        // built-in 1 s self-heal / debug force: verify by signature. The baseline
        // (mListSig) is refreshed at the END of a rebuild, so a 2 Hz-capped early
        // return cannot swallow a detected difference.
        if (listSignature() == mListSig)
        {
            mListGate.accept(revisions); // verified current; restart the fallback timer
            stale = false;
        }
    }
    if (!stale)
    {
        return;
    }
    // Rule 3: a list with more than 50 rows refreshes at most 2 Hz. A handler that
    // just mutated the model passes force_now and always gets its rows immediately.
    if (!force_now && mList->getItemCount() > 50 && !mListCapClock.due())
    {
        return;
    }
    mListGate.accept(revisions);
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost rebuild list");

    const LLUUID prev_sel = selectedListValue();
    mList->deleteAllItems();
    std::set<LLUUID> emitted_groups;

    auto add_instance_row =
        [this](const ALGhostStudio::Instance& inst, bool indented)
    {
        LLSD row;
        row["value"] = inst.mId;
        row["columns"][0]["column"] = "on";
        row["columns"][0]["value"] =
            inst.mEnabled ? "\xE2\x97\x8F" : "\xE2\x97\x8B";
        row["columns"][1]["column"] = "name";
        row["columns"][1]["value"] =
            (indented ? std::string("\xE2\x94\x94 ") : std::string()) +
            inst.mName;
        row["columns"][2]["column"] = "kind";
        row["columns"][2]["value"] = kind_name(inst.mKind);
        row["columns"][3]["column"] = "state";
        row["columns"][3]["value"] = state_name(inst);
        row["columns"][4]["column"] = "source";
        row["columns"][4]["value"] = inst.mSourceLabel.empty()
            ? sourceName(inst.mSource) : inst.mSourceLabel;
        row["columns"][5]["column"] = "style";
        row["columns"][5]["value"] =
            inst.mKind == ALGhostStudio::BACKING_ENTITY_CLONE
                ? std::string("Scene-lit")
                : std::string(style_name(inst.mStyle));
        row["columns"][6]["column"] = "pose";
        row["columns"][6]["value"] =
            inst.mPose == ALGhostStudio::POSE_FROZEN
                ? std::string("FROZEN") : std::string("live");
        mList->addElement(row, ADD_BOTTOM);
    };

    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(inst.mId);
        if (!group)
        {
            add_instance_row(inst, false);
            continue;
        }
        if (!emitted_groups.insert(group->mId).second)
        {
            continue;
        }

        bool all_enabled = true;
        for (const ALGhostGroupModel::Member& member : group->mMembers)
        {
            const ALGhostStudio::Instance* child =
                studio.getInstance(member.mInstanceId);
            all_enabled = all_enabled && child && child->mEnabled;
        }

        LLSD header;
        header["value"] = group->mId;
        header["columns"][0]["column"] = "on";
        header["columns"][0]["value"] =
            all_enabled ? "\xE2\x97\x8F" : "\xE2\x97\x8B";
        header["columns"][1]["column"] = "name";
        header["columns"][1]["value"] =
            std::string(group->mEditMembers ? "\xE2\x96\xBC " :
                                              "\xE2\x96\xB6 ") +
            group->mName + llformat(" (%d)", (S32)group->mMembers.size());
        header["columns"][2]["column"] = "kind";
        header["columns"][2]["value"] = "Group";
        header["columns"][3]["column"] = "state";
        header["columns"][3]["value"] =
            group->mMode == ALGhostGroupModel::MODE_MOVE_FACE
                ? "Move+Face" : "Rigid";
        header["columns"][4]["column"] = "source";
        header["columns"][4]["value"] = "";
        header["columns"][5]["column"] = "style";
        header["columns"][5]["value"] = "";
        header["columns"][6]["column"] = "pose";
        header["columns"][6]["value"] = "group";
        mList->addElement(header, ADD_BOTTOM);

        if (group->mEditMembers)
        {
            for (const ALGhostGroupModel::Member& member : group->mMembers)
            {
                if (const ALGhostStudio::Instance* child =
                        studio.getInstance(member.mInstanceId))
                {
                    add_instance_row(*child, true);
                }
            }
        }
    }
    if (prev_sel.notNull())
    {
        mList->selectByID(prev_sel);
    }
    noteListSelectionChanged(); // rows were rebuilt: cached selection is stale
    mListSig = listSignature(); // fallback diff baseline
}

void ALPanelGhostStudio::refreshDetail()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    // [GhostPanelPerf] cached selection: no vector/set builds per frame
    const SelectionCache& selected = selection();
    const LLUUID row_value = selected.mRow;
    const LLUUID sel = selected.mInstance;
    ALGhostStudio::Instance* inst = studio.getInstance(sel);
    const bool have = inst != nullptr;
    const bool overlay = have && inst->mKind == ALGhostStudio::BACKING_OVERLAY;
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(row_value);
    const bool group_header = group && row_value == group->mId;
    const bool member_edit =
        group && group->mEditMembers && !group_header;
    const bool any_group_selected = selected.mAnyGroup;
    const ALGhostStudio::CrowdPlacementDraft& crowd_draft =
        studio.getCrowdPlacementDraft();
    const bool crowd_active = crowd_draft.mActive;

    // [GhostPanelPerf] GATE. The enable/visible block and the value mirrors only
    // run when (a) the selection / instance model / crowd-draft key changed, (b) a
    // commit handler asked for it, or (c) the 10 Hz readout tick is due. The key is
    // built from model state only (no widget reads, no allocations).
    const U64 key_flags =
        (crowd_active ? 1ULL : 0ULL) |
        (mEditMode ? 2ULL : 0ULL) |
        (mHasTransformClipboard ? 4ULL : 0ULL) |
        (mActiveStrip != 0 ? 8ULL : 0ULL) |
        (have ? 16ULL : 0ULL) |
        ((have && inst->mChaosEnabled) ? 32ULL : 0ULL) |
        ((have && inst->mPose == ALGhostStudio::POSE_FROZEN) ? 64ULL : 0ULL) |
        (group ? 128ULL : 0ULL) |
        (member_edit ? 256ULL : 0ULL) |
        (group_header ? 512ULL : 0ULL) |
        ((have && inst->mEntityId.notNull()) ? 1024ULL : 0ULL) |
        (have ? ((U64)inst->mKind + 1ULL) << 12 : 0ULL) |
        (have ? ((U64)inst->mDriveMode + 1ULL) << 16 : 0ULL);
    const bool refresh_always = ghost_refresh_always();
    const bool key_changed = mDetailGate.changed(
        { selected.mStamp, key_flags, crowd_draft.mRevision }, refresh_always);
    if (!key_changed && !mReadoutTick && !mDetailDirty)
    {
        return;
    }
    mDetailDirty = false;
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost refreshDetail apply");
    // strings / metadata / pose status refresh at 4 Hz (or immediately on a key change)
    const bool slow_tick = key_changed || mStatusTick;

    // enables (the add row is always live; everything else needs a selection)
    setEnabledIfChanged(mDupBtn, group_header || overlay);
    setEnabledIfChanged(mDelBtn, have && !member_edit);
    setEnabledIfChanged(mNameEdit, have);
    setEnabledIfChanged(mPosX, have);
    setEnabledIfChanged(mPosY, have);
    setEnabledIfChanged(mPosZ, have);
    setEnabledIfChanged(mHeadingDial, have);
    setEnabledIfChanged(mYawSpin, have);
    setEnabledIfChanged(mScaleSpin, have);
    setEnabledIfChanged(mLookTargetCombo, have);
    setEnabledIfChanged(mFaceNowBtn, have);
    setEnabledIfChanged(mKeepFacingCheck, have);
    const bool entity = have && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
    const bool directed = entity &&
        inst->mDriveMode == ALGhostStudio::DRIVE_DIRECTED;
    const bool show_style_controls = !entity;
    setVisibleIfChanged(mLookSection, show_style_controls);
    setVisibleIfChanged(mEntityStyleHint, !show_style_controls);
    setVisibleIfChanged(mRefreshBtn, entity);
    setEnabledIfChanged(mRefreshBtn, entity);
    std::string type = "Type: No selection";
    if (overlay)
    {
        type = "Type: Overlay ghost";
    }
    else if (entity)
    {
        type = std::string("Type: Entity clone \xC2\xB7 ") + state_name(*inst);
    }
    setTextIfChanged(mTypeText, type);
    setValueIfChanged(mCrowdCopiesAs,
        LLSD(crowd_active
            ? (S32)crowd_draft.mSource.mKind
            : (have ? (S32)inst->mKind : -1)));
    setEnabledIfChanged(mCrowdCopiesAs, false);
    setEnabledIfChanged(mChaosCheck, entity && !any_group_selected);
    setEnabledIfChanged(mChaosSlider,
        entity && inst->mChaosEnabled && !any_group_selected);
    setEnabledIfChanged(mAnimSpeedSpin, entity);
    refreshAutoAnimate(inst); // [AutoAnimate]
    setEnabledIfChanged(mPhysicsCheck, entity);
    setEnabledIfChanged(mAnimPauseBtn, entity);
    setEnabledIfChanged(mAnimResumeBtn, entity);
    setEnabledIfChanged(mEntityLookCombo, entity);
    setEnabledIfChanged(mDriveModeCombo, entity);
    setVisibleIfChanged(mDirectedAnimEdit, directed);
    setEnabledIfChanged(mDirectedAnimEdit, directed);
    setVisibleIfChanged(mAnimationLibraryCombo, directed);
    setVisibleIfChanged(mAnimationLibraryRefreshBtn, directed);
    setEnabledIfChanged(mAnimationLibraryCombo, directed);
    setVisibleIfChanged(mAnimMetadataText, directed);
    setEnabledIfChanged(mLoopModeCombo, entity);
    setEnabledIfChanged(mAnimSyncBtn, entity);
    setEnabledIfChanged(mLensGazeCheck, entity && inst->mEntityId.notNull());
    const bool have_selected_entity = selected.mAnyEntity;
    setEnabledIfChanged(mLensGazeSelectionBtn, have_selected_entity);
    setEnabledIfChanged(mLensGazeTorsoSlider, have_selected_entity);
    setEnabledIfChanged(mLensGazeTargetMode, have_selected_entity);
    setEnabledIfChanged(mLensGazeCastTarget, have_selected_entity &&
        mLensGazeTargetMode->getValue().asInteger() == LLActorMover::GAZE_CAST);
    setEnabledIfChanged(mLensGazeHeadEyeSlider, have_selected_entity);
    setEnabledIfChanged(mLensGazeIntensitySlider, have_selected_entity);
    setEnabledIfChanged(mLensGazeSmoothingSlider, have_selected_entity);
    setEnabledIfChanged(mPlaceBtn, overlay && !crowd_active);
    setEnabledIfChanged(mEditModeCheck, !crowd_active || mEditMode);
    setEnabledIfChanged(mToActorBtn, overlay);
    setEnabledIfChanged(mToMeBtn, overlay);
    setEnabledIfChanged(mStyleCombo, overlay);
    setEnabledIfChanged(mActorTintCheck, overlay);
    setEnabledIfChanged(mHueSlider, overlay && !mActorTintCheck->get());
    setEnabledIfChanged(mAlphaSlider, overlay);
    setEnabledIfChanged(mShimmerSpeedSlider, overlay);
    setEnabledIfChanged(mShimmerAmountSlider, overlay);
    setEnabledIfChanged(mPixelSlider, overlay);
    setEnabledIfChanged(mGlitchSlider, overlay);
    setEnabledIfChanged(mDistortCombo, overlay);
    setEnabledIfChanged(mDistortAmountSlider,
        overlay && mDistortCombo->getValue().asInteger() != 0);
    setEnabledIfChanged(mBrightnessSlider, overlay);
    setEnabledIfChanged(mEffectFps, overlay);
    setEnabledIfChanged(mLockMode, group != nullptr);
    setEnabledIfChanged(mFreezeBtn, overlay);
    setEnabledIfChanged(mLiveBtn,
        overlay && inst->mPose == ALGhostStudio::POSE_FROZEN);
    setEnabledIfChanged(mArrayCount, overlay);
    setEnabledIfChanged(mArraySpacing, overlay);
    setEnabledIfChanged(mArrayLineBtn, overlay);
    setEnabledIfChanged(mArrayRingBtn, overlay);
    setEnabledIfChanged(mDropBtn, have);
    setEnabledIfChanged(mAlignFeetBtn, have);
    setEnabledIfChanged(mUprightBtn, have);
    setEnabledIfChanged(mDupInPlaceBtn, have);
    setEnabledIfChanged(mCopyTransformBtn, have);
    setEnabledIfChanged(mPasteTransformBtn, have && mHasTransformClipboard);
    setEnabledIfChanged(mArrayCount, have);
    setEnabledIfChanged(mArraySpacing, have);
    setEnabledIfChanged(mArrayLineBtn, have);
    setEnabledIfChanged(mArrayRingBtn, have);
    setEnabledIfChanged(mFormationCombo, have);
    setEnabledIfChanged(mArrayBuildBtn, have);
    const ALGhostStudio::EFormation crowd_formation =
        (ALGhostStudio::EFormation)mFormationCombo->getValue().asInteger();
    setEnabledIfChanged(mStripStartBtn,
        have && formationSupportsFreezeStrip(crowd_formation));
    setEnabledIfChanged(mStripCancelBtn, mActiveStrip != 0);
    setEnabledIfChanged(mMotionApplyBtn, have && !any_group_selected);
    setEnabledIfChanged(mGroupEditMembersCheck, group != nullptr);
    setValueIfChanged(mGroupEditMembersCheck,
        LLSD(group && group->mEditMembers), true);
    setEnabledIfChanged(mGroupMemberPinnedCheck, member_edit);
    setEnabledIfChanged(mGroupMemberResetBtn, member_edit);
    bool member_pinned = false;
    if (member_edit)
    {
        for (const ALGhostGroupModel::Member& member : group->mMembers)
        {
            if (member.mInstanceId == sel)
            {
                member_pinned = member.mPinned;
                break;
            }
        }
    }
    setValueIfChanged(mGroupMemberPinnedCheck,
        LLSD(member_pinned), true);
    if (group_header)
    {
        // setTextIfChanged is focus-aware: it never overwrites a name being typed
        setTextIfChanged(mNameEdit, group->mName);
    }

    if (!have)
    {
        if (mShownFor.notNull())
        {
            mShownFor.setNull();
            setTextIfChanged(mNameEdit, std::string(), true);
            setTextIfChanged(mPoseStatus, std::string("Select a ghost"), true);
        }
        return;
    }

    LLVector3d unit_foot = inst->mFootGlobal;
    LLQuaternion unit_rotation = inst->mRotation;
    F32 unit_scale = inst->mScale;
    bool collapsed_group = false;
    selectedUnitTransform(
        unit_foot, unit_rotation, unit_scale, collapsed_group);
    const LLVector3 unit_forward =
        LLVector3::x_axis * unit_rotation;
    const F32 unit_yaw_degrees =
        atan2f(unit_forward.mV[VY], unit_forward.mV[VX]) * RAD_TO_DEG;
    F32 scale_minimum = GHOST_SCALE_MIN;
    F32 scale_maximum = GHOST_SCALE_MAX;
    const bool have_group_scale_limits =
        !collapsed_group ||
        studio.getGroupScaleLimits(
            row_value, scale_minimum, scale_maximum);
    setEnabledIfChanged(mScaleSpin, have && have_group_scale_limits);
    // [GhostPanelPerf] limits / increment / tooltip are rewritten only when their
    // inputs change (the tooltip used to be re-formatted EVERY frame).
    if (!mScaleStateValid || mScaleMinShown != scale_minimum ||
        mScaleMaxShown != scale_maximum ||
        mScaleCollapsedShown != collapsed_group)
    {
        mScaleSpin->setMinValue(scale_minimum);
        setMaxValueIfChanged(mScaleSpin, scale_maximum); // range before the value write below
        mScaleSpin->setIncrement(collapsed_group ? 0.01f : 0.05f);
        setToolTipIfChanged(mScaleSpin, collapsed_group
            ? llformat(
                "Group scale factor. Allowed range: %.6g to %.6g. "
                "The authored group pivot and every member's safe size are preserved.",
                scale_minimum, scale_maximum)
            : std::string("Uniform size, pivoted at the feet so they stay planted "
                          "(1 = life size)"));
        mScaleMinShown = scale_minimum;
        mScaleMaxShown = scale_maximum;
        mScaleCollapsedShown = collapsed_group;
        mScaleStateValid = true;
        mScalePrecisionPending = true;
    }
    if (mScalePrecisionPending && !alIsBeingEdited(mScaleSpin))
    {
        // setPrecision() rewrites LLSpinCtrl's inline editor; never call it
        // over text the operator is actively typing.
        mScaleSpin->setPrecision(collapsed_group ? 4 : 2);
        mScalePrecisionPending = false;
    }

    // POSITION mirrors at the 10 Hz readout rate (a LIVE ghost never moves itself,
    // but Place / snaps / arrays move it from outside the spinners). Every write is
    // focus-aware and diff-guarded: a spinner the operator is typing in is skipped.
    {
        const LLVector3 agent_pos =
            gAgent.getPosAgentFromGlobal(unit_foot);
        setValueIfChanged(mPosX, LLSD((F64)agent_pos.mV[VX]));
        setValueIfChanged(mPosY, LLSD((F64)agent_pos.mV[VY]));
        setValueIfChanged(mPosZ, LLSD((F64)agent_pos.mV[VZ]));
    }
    // [GhostPanelPerf] No outer Scale-focus guard: each helper guards ITS OWN widget
    // (descendants included), so a focused Scale spinner never freezes the unrelated
    // Chaos / anim speed / physics / entity-look mirrors.
    setValueIfChanged(mScaleSpin, LLSD((F64)unit_scale));
    setValueIfChanged(mChaosCheck, LLSD(inst->mChaosEnabled), true);
    setValueIfChanged(mChaosSlider, LLSD((F64)inst->mChaosAmount));
    setValueIfChanged(mAnimSpeedSpin, LLSD((F64)inst->mAnimSpeed));
    setValueIfChanged(mPhysicsCheck, LLSD(inst->mPhysicsEnabled), true);
    setValueIfChanged(mEntityLookCombo, LLSD((S32)inst->mLook));
    setValueIfChanged(mEffectFps, LLSD((F64)inst->mEffectFps));
    if (inst->mGroupId.notNull() && !studio.getCrowdPlacementDraft().mActive)
        setValueIfChanged(mLockMode, LLSD((S32)inst->mLockMode));
    if (!mYawSpin->hasFocus() && !mHeadingDial->hasMouseCapture())
    {
        // Aim commands, continuous tracking, and the in-world manip proxy can
        // all change heading outside these controls. Mirror at the readout rate
        // (while preserving an active edit) so the dial and exact spinner never lie.
        setValueIfChanged(mHeadingDial, LLSD((F64)unit_yaw_degrees));
        setValueIfChanged(mYawSpin, LLSD((F64)unit_yaw_degrees));
    }
    setValueIfChanged(mKeepFacingCheck, LLSD(inst->mKeepFacing),
                      true);
    if (entity)
    {
        setValueIfChanged(mDriveModeCombo, LLSD((S32)inst->mDriveMode));
        // no LLUUID::asString() per tick: format only when the model value moved
        // (or the selection/key changed)
        if (key_changed || mDirectedAnimShown != inst->mDirectedAnim)
        {
            setTextIfChanged(mDirectedAnimEdit, inst->mDirectedAnim.asString());
            mDirectedAnimShown = inst->mDirectedAnim;
        }
    }
    setValueIfChanged(mLoopModeCombo, LLSD((S32)inst->mLoopMode));
    if (entity)
    {
        LLActorMover& mover = LLActorMover::instance();
        setValueIfChanged(mLensGazeCheck,
            LLSD(inst->mEntityId.notNull() &&
                 mover.isGazeEnabled(inst->mEntityId)),
            true);
        setValueIfChanged(mLensGazeTargetMode,
            LLSD((S32)mover.getGazeTargetMode(inst->mEntityId)));
        setValueIfChanged(mLensGazeCastTarget,
            LLSD(mover.getGazeCastTarget(inst->mEntityId)));
        // sliders: the helper itself skips a control under mouse capture / focus
        setValueIfChanged(mLensGazeTorsoSlider,
            LLSD((F64)mover.getGazeTorsoAmount(inst->mEntityId)));
        setValueIfChanged(mLensGazeHeadEyeSlider,
            LLSD((F64)mover.getGazeHeadEyeBlend(inst->mEntityId)));
        setValueIfChanged(mLensGazeIntensitySlider,
            LLSD((F64)mover.getGazeIntensity(inst->mEntityId)));
        setValueIfChanged(mLensGazeSmoothingSlider,
            LLSD((F64)mover.getGazeSmoothing(inst->mEntityId)));
        if (slow_tick)
        {
            std::string gaze_status;
            mover.getGazeStatus(inst->mEntityId, gaze_status);
            setTextIfChanged(mLensGazeStatus, gaze_status);
        }
    }

    if (directed && slow_tick)
    {
        // library: lazy, shared, never scanned here. Ask once; mirror when it changes.
        if (!studio.animLibraryScanned())
        {
            studio.requestAnimLibraryScan();
        }
        else if (studio.animLibraryRevision() != mAnimLibraryShownRev)
        {
            populateAnimationLibrary();
        }
        ALGhostAnimAssetIndex& index = ALGhostAnimAssetIndex::instance();
        if (inst->mDirectedAnim.notNull())
        {
            setValueIfChanged(mAnimationLibraryCombo, LLSD(inst->mDirectedAnim));
            index.requestMetadata(inst->mDirectedAnim);
        }
        const auto* info = index.find(inst->mDirectedAnim);
        std::string metadata = "Metadata unavailable";
        if (inst->mDirectedAnim.isNull())
        {
            metadata = "Choose an owned inventory animation or paste a UUID";
        }
        else if (info && info->mAvailability == ALGhostAnimAssetIndex::AVAILABLE_PENDING)
        {
            metadata = "Metadata pending...";
        }
        else if (info && info->mAvailability == ALGhostAnimAssetIndex::AVAILABLE_READY)
        {
            metadata = llformat(
                "%.2fs | asset loop %s %.2f-%.2f | ease %.2f/%.2f | "
                "priority %d | %d joints",
                info->mDuration, info->mLoop ? "yes" : "no",
                info->mLoopIn, info->mLoopOut, info->mEaseIn, info->mEaseOut,
                info->mPriority, (S32)info->mJoints.size());
            if (!info->mHandPoseName.empty())
            {
                metadata += " | hand " + info->mHandPoseName;
            }
            if (!info->mEmoteName.empty())
            {
                metadata += " | emote " + info->mEmoteName;
            }
            if (!info->mJoints.empty())
            {
                metadata += "\nJoints: ";
                for (const std::string& joint : info->mJoints)
                {
                    if (metadata.back() != ' ') metadata += ", ";
                    metadata += joint;
                }
            }
        }
        setTextIfChanged(mAnimMetadataText, metadata);
    }

    // [GhostPanelPerf] EVERY model-backed detail field is mirrored on each detail
    // run (revision/key change or the 10 Hz tick) through the focus-guarded diff
    // helpers, so an edit made from the OTHER visible host (rename, style, tint,
    // alpha, distortion, look target...) lands within one tick. `force` bypasses the
    // focus guard ONLY when the selection identity changed.
    const bool identity_changed = row_value != mShownFor;
    mShownFor = row_value;
    const bool force = identity_changed;
    setTextIfChanged(mNameEdit, group_header ? group->mName : inst->mName, force);
    setValueIfChanged(mStyleCombo, LLSD((S32)inst->mStyle), force);
    setValueIfChanged(mActorTintCheck, LLSD(inst->mUseActorTint), true);
    setValueIfChanged(mHueSlider, LLSD((F64)inst->mHue), force);
    setValueIfChanged(mAlphaSlider, LLSD((F64)inst->mAlpha), force);
    setValueIfChanged(mShimmerSpeedSlider, LLSD((F64)inst->mShimmerSpeed), force);
    setValueIfChanged(mShimmerAmountSlider, LLSD((F64)inst->mShimmerIntensity), force);
    setValueIfChanged(mPixelSlider, LLSD((F64)inst->mPixelSize), force);
    setValueIfChanged(mGlitchSlider, LLSD((F64)inst->mGlitch), force);
    setValueIfChanged(mDistortCombo, LLSD((S32)inst->mDistort), force);
    setValueIfChanged(mDistortAmountSlider, LLSD((F64)inst->mDistortAmount), force);
    setValueIfChanged(mBrightnessSlider, LLSD((F64)inst->mBrightness), force);
    if (force || slow_tick || mShownLookTarget != (S32)inst->mLookTarget ||
        mShownLookTargetId != inst->mLookTargetId)
    {
        // formatted only when its model inputs moved (or at the 4 Hz slow tick)
        std::string target = "camera";
        if (inst->mLookTarget == ALGhostStudio::LOOK_TARGET_ME)
            target = "me";
        else if (inst->mLookTarget == ALGhostStudio::LOOK_TARGET_ACTOR)
            target = "actor:" + inst->mLookTargetId.asString();
        else if (inst->mLookTarget == ALGhostStudio::LOOK_TARGET_GHOST)
            target = "ghost:" + inst->mLookTargetId.asString();
        setValueIfChanged(mLookTargetCombo, LLSD(target), force);
        mShownLookTarget = (S32)inst->mLookTarget;
        mShownLookTargetId = inst->mLookTargetId;
    }
    if (identity_changed)
    {
        // controls mirrored above only while NOT focused get a forced resync here
        setValueIfChanged(mHeadingDial, LLSD((F64)unit_yaw_degrees), true);
        setValueIfChanged(mYawSpin, LLSD((F64)unit_yaw_degrees), true);
        setValueIfChanged(mScaleSpin, LLSD((F64)unit_scale), true);
        setValueIfChanged(mAnimSpeedSpin, LLSD((F64)inst->mAnimSpeed), true);
        setValueIfChanged(mPhysicsCheck, LLSD(inst->mPhysicsEnabled), true);
        setValueIfChanged(mDriveModeCombo, LLSD((S32)inst->mDriveMode), true);
        setTextIfChanged(mDirectedAnimEdit, inst->mDirectedAnim.asString(), true);
        mDirectedAnimShown = inst->mDirectedAnim;
        setValueIfChanged(mEffectFps, LLSD((F64)inst->mEffectFps), true);
        // the combo values the enable block reads just changed
        mDetailDirty = true;
    }
    // pose status: what the ghost is doing, and why a freeze might not bite
    if (slow_tick)
    {
        std::string pose;
        if (inst->mPose == ALGhostStudio::POSE_FROZEN)
        {
            pose = "FROZEN pose held";
        }
        else if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            pose = "Scene-lit entity; overlay style controls unavailable";
        }
        else
        {
            LLVOAvatar* av = LLDirectorCast::instance().resolve(inst->mSource);
            pose = (av && LLActorMover::instance().ghostBatchesFor(av->getID()))
                 ? "Following live pose"
                 : "Source not rendering (no ghost)";
        }
        setTextIfChanged(mPoseStatus, pose);
    }
}

void ALPanelGhostStudio::refreshStatus()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    // [GhostPanelPerf] status strings: 4 Hz, or immediately when the instance model moves
    const U64 instance_rev = studio.getInstanceRevision();
    if (!mStatusTick && instance_rev == mStatusInstanceRev &&
        !ghost_refresh_always())
    {
        return;
    }
    mStatusInstanceRev = instance_rev;
    S32 shown = 0;
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        if (inst.mEnabled)
        {
            ++shown;
        }
    }
    std::string txt = llformat("%d ghost(s), %d shown%s",
                               (S32)studio.getInstances().size(), shown,
                               studio.getShowAll() ? "" : " (all hidden)");
    setTextIfChanged(mStatusText, txt);
    std::string strip = studio.freezeStripStatus();
    const ALGhostStudio::EFormation formation =
        (ALGhostStudio::EFormation)mFormationCombo->getValue().asInteger();
    if (!mActiveStrip && !formationSupportsFreezeStrip(formation))
    {
        strip = "Freeze Strip unavailable for this formation; "
                "choose a classic layout above.";
    }
    setTextIfChanged(mStripStatus, strip);
    if (mActiveStrip && strip.find(llformat("Strip %u:", mActiveStrip)) != 0)
    {
        mActiveStrip = 0;
        setEnabledIfChanged(mStripCancelBtn, false);
    }
}

// [GhostPanelPerf] Rig diagnostics are ON DEMAND, not polled. The text is rebuilt
// only when ALL of these hold:
//   * something is known stale: first build, selection changed, the selected
//     clone's runtime changed, the 0.5 Hz structure re-snapshot is due (the text reads
//     async drawables/faces/TEs no revision tracks), or Refresh was pressed;
//   * the editor is actually on screen (isWidgetOnScreen -- the same visibility
//     and rect test LLView::drawChildren applies, so a editor scrolled out of its
//     host costs nothing and fills on its first on-screen frame);
//   * the operator is not selecting/copying text in it.
// The auto text is the STRUCTURE snapshot (capped); the camera-dependent
// render/cull columns appear only in a Refresh snapshot. GhostStudioRefreshAlways
// restores the old 0.5 s live rebuild regardless of visibility.
void ALPanelGhostStudio::refreshRigDiagnostics()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const bool always = ghost_refresh_always();
    const LLUUID& selection_id = studio.getSelected();
    const ALGhostStudio::Instance* instance = studio.getInstance(selection_id);
    const bool entity_clone =
        instance && instance->mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
    const LLUUID entity_id = entity_clone ? instance->mEntityId : LLUUID::null;

    bool live = mRigDiagRefreshRequested;
    bool stale = live || !mRigDiagShown ||
                 selection_id != mRigDiagShownSelection ||
                 entity_id != mRigDiagShownEntity;
    if (always)
    {
        if (!stale && mRigDiagnosticsTimer.getElapsedTimeF32() < 0.5f)
        {
            return;
        }
        mRigDiagnosticsTimer.reset();
        live = true;
        stale = true;
    }
    else if (!stale)
    {
        // The structure text reads asynchronously created drawables / faces / TEs
        // that no revision tracks, so re-snapshot at 0.5 Hz -- but only while the
        // editor is on screen (checked below) and only for an entity clone. The text
        // is camera-independent and capped, and setTextIfChanged skips an identical
        // result, so an unchanged clone costs one capped string build per 2 s.
        if (!entity_clone || !mRigDiagPollClock.due())
        {
            return;
        }
    }    if (!always)
    {
        if (!isWidgetOnScreen(mRigDiagnostics))
        {
            return; // stays stale; refreshed on the first on-screen frame
        }
        if (!live && mRigDiagnostics->hasFocus())
        {
            return; // never pull text out from under a selection
        }
    }

    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost rig diagnostics refresh");
    std::string text =
        "Select an entity clone to inspect rig and per-face texture state.";
    U64 clone_revision = 0;
    if (entity_clone)
    {
        LLGhostAvatar* ghost = studio.resolveEntityClone(instance->mId);
        if (ghost)
        {
            text = ghost->getRigDiagnosticText(live);
            clone_revision = ghost->getRigDiagRevision();
        }
        else
        {
            text = "Selected entity clone is not currently available.";
        }
    }
    setTextIfChanged(mRigDiagnostics, text, true);
    mRigDiagShown = true;
    mRigDiagShownSelection = selection_id;
    mRigDiagShownEntity = entity_id;
    mRigDiagShownCloneRev = clone_revision;
    mRigDiagRefreshRequested = false;
}

void ALPanelGhostStudio::onClickRigDiagRefresh()
{
    // one snapshot WITH the live render/cull columns; applied by the next draw if on screen
    mRigDiagRefreshRequested = true;
}

// ---------------------------------------------------------------------------
// list + CRUD
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onListSelect()
{
    // [R2-3] the list drives the SHARED selection (tool + both hosts follow);
    // refreshDetail() loads the widgets on the next draw
    noteListSelectionChanged(); // [GhostPanelPerf] user changed the list selection
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row_value = selectedListValue();
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(row_value);
    studio.setSelected(
        group && row_value == group->mId
            ? group->mId : selectedInstance());
    mObservedSelectionRevision = studio.getSelectionRevision();
    syncCrowdFacingPointPrototype();
}

void ALPanelGhostStudio::onListMouseUp(S32 x, S32, MASK mask)
{
    if (mask != MASK_NONE || mList->getColumnIndexFromOffset(x) != 0)
    {
        return;
    }

    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row_value = selectedListValue();
    if (const ALGhostGroupModel::Group* group =
            studio.groupForMember(row_value);
        group && row_value == group->mId)
    {
        bool all_enabled = true;
        for (const ALGhostGroupModel::Member& member : group->mMembers)
        {
            const ALGhostStudio::Instance* inst =
                studio.getInstance(member.mInstanceId);
            all_enabled = all_enabled && inst && inst->mEnabled;
        }
        for (const ALGhostGroupModel::Member& member : group->mMembers)
        {
            studio.setInstanceEnabled(member.mInstanceId, !all_enabled);
        }
        refreshList(true);
    }
    else if (ALGhostStudio::Instance* inst =
                 studio.getInstance(selectedInstance()))
    {
        studio.setInstanceEnabled(inst->mId, !inst->mEnabled);
        // The enabled bit is part of mListSig, so this rebuilds the dot now
        // while preserving the selected instance by UUID.
        refreshList(true);
    }
}

void ALPanelGhostStudio::onListDoubleClick()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row_value = selectedListValue();
    if (const ALGhostGroupModel::Group* group =
            studio.groupForMember(row_value))
    {
        const LLUUID representative = selectedInstance();
        const bool editing = !group->mEditMembers;
        studio.setGroupEditMembers(group->mId, editing);
        studio.setSelected(editing ? group->mId : representative);
        mListGate.reset(); // [GhostPanelPerf] force a rebuild on the next refreshList()
        return;
    }
    if (ALGhostStudio::Instance* inst = studio.getInstance(selectedInstance()))
    {
        studio.setInstanceEnabled(inst->mId, !inst->mEnabled);
    }
}

void ALPanelGhostStudio::onClickAdd()
{
    const LLUUID source = mSourceCombo->getSelectedValue().asUUID();
    ALGhostStudio::Instance* inst = nullptr;
    const S32 kind = mCloneTypeCombo->getValue().asInteger();

    if (kind == ALGhostStudio::BACKING_ENTITY_CLONE)
    {
        inst = ALGhostStudio::instance().spawnEntityClone(source, sourceName(source));
    }
    else
    {
        inst = ALGhostStudio::instance().addInstance(source);
    }
    if (inst)
    {
        const LLUUID id = inst->mId;    // list rebuild invalidates the pointer
        ALGhostStudio::instance().setSelected(id);
        refreshList(true);
        mList->selectByID(id);
        noteListSelectionChanged();
    }
}

void ALPanelGhostStudio::onNameCommit()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row_value = selectedListValue();
    if (const ALGhostGroupModel::Group* group =
            studio.groupForMember(row_value);
        group && row_value == group->mId)
    {
        studio.renameGroup(group->mId, mNameEdit->getText());
    }
    else
    {
        studio.renameInstance(selectedInstance(), mNameEdit->getText());
    }
    mListGate.reset(); // [GhostPanelPerf] force a rebuild on the next refreshList()
}

void ALPanelGhostStudio::onClickDuplicate()
{
    std::vector<LLUUID> duplicate_rows;
    LLUUID first_selection;
    std::set<LLUUID> handled;
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::set<LLUUID> selected_group_headers;
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID value = item->getValue().asUUID();
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group && value == group->mId)
        {
            selected_group_headers.insert(group->mId);
        }
    }
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID value = item->getValue().asUUID();
        if (!handled.insert(value).second)
        {
            continue;
        }
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group && value != group->mId &&
            selected_group_headers.find(group->mId) !=
                selected_group_headers.end())
        {
            continue;
        }
        if (group && value == group->mId)
        {
            const std::vector<LLUUID> copies =
                studio.duplicateGroup(group->mId, false);
            if (!copies.empty())
            {
                const ALGhostStudio::Instance* first =
                    studio.getInstance(copies.front());
                if (first)
                {
                    duplicate_rows.push_back(first->mGroupId);
                    if (first_selection.isNull())
                        first_selection = first->mGroupId;
                }
            }
        }
        else if (ALGhostStudio::Instance* inst =
                     studio.duplicateInstance(value))
        {
            duplicate_rows.push_back(inst->mId);
            if (first_selection.isNull()) first_selection = inst->mId;
        }
    }
    if (!duplicate_rows.empty())
    {
        studio.setSelected(first_selection);
        refreshList(true);
        mList->deselectAllItems(true);
        mList->selectMultiple(duplicate_rows);
        noteListSelectionChanged();
        mObservedSelectionRevision =
            studio.getSelectionRevision();
    }
}

void ALPanelGhostStudio::onClickDuplicateInPlace()
{
    std::vector<LLUUID> duplicate_rows;
    LLUUID first_selection;
    std::set<LLUUID> handled;
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::set<LLUUID> selected_group_headers;
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID value = item->getValue().asUUID();
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group && value == group->mId)
        {
            selected_group_headers.insert(group->mId);
        }
    }
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID value = item->getValue().asUUID();
        if (!handled.insert(value).second)
        {
            continue;
        }
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group && value != group->mId &&
            selected_group_headers.find(group->mId) !=
                selected_group_headers.end())
        {
            continue;
        }
        if (group && value == group->mId)
        {
            const std::vector<LLUUID> copies =
                studio.duplicateGroup(group->mId, true);
            if (!copies.empty())
            {
                const ALGhostStudio::Instance* first =
                    studio.getInstance(copies.front());
                if (first)
                {
                    duplicate_rows.push_back(first->mGroupId);
                    if (first_selection.isNull())
                        first_selection = first->mGroupId;
                }
            }
        }
        else if (ALGhostStudio::Instance* inst =
                     studio.duplicateInstanceInPlace(value))
        {
            duplicate_rows.push_back(inst->mId);
            if (first_selection.isNull()) first_selection = inst->mId;
        }
    }
    if (!duplicate_rows.empty())
    {
        studio.setSelected(first_selection);
        refreshList(true);
        mList->deselectAllItems(true);
        mList->selectMultiple(duplicate_rows);
        noteListSelectionChanged();
        mObservedSelectionRevision =
            studio.getSelectionRevision();
    }
}

void ALPanelGhostStudio::onClickDelete()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::set<LLUUID> removed_units;
    bool removed_any = false;
    for (LLScrollListItem* item : mList->getAllSelected())
    {
        const LLUUID value = item->getValue().asUUID();
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group)
        {
            if (value != group->mId)
            {
                // Expanded child rows are positioning controls, not implicit
                // whole-crowd delete buttons.
                continue;
            }
            if (!removed_units.insert(group->mId).second ||
                group->mMembers.empty())
            {
                continue;
            }
            studio.removeInstance(group->mMembers.front().mInstanceId);
            removed_any = true;
        }
        else if (removed_units.insert(value).second)
        {
            studio.removeInstance(value);
            removed_any = true;
        }
    }
    if (!removed_any)
    {
        return;
    }
    studio.setSelected(LLUUID::null);
    refreshList(true);
    mList->deselectAllItems(true);
    noteListSelectionChanged();
    mShownFor.setNull();
}

void ALPanelGhostStudio::onClickRefresh()
{
    ALGhostStudio::instance().refreshEntityClone(selectedInstance());
}

// ---------------------------------------------------------------------------
// placement
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onPosCommit()
{
    LLVector3d foot;
    LLQuaternion rotation;
    F32 scale = 1.f;
    bool collapsed_group = false;
    if (selectedUnitTransform(
            foot, rotation, scale, collapsed_group))
    {
        const LLVector3 agent_pos((F32)mPosX->getValue().asReal(),
                                  (F32)mPosY->getValue().asReal(),
                                  (F32)mPosZ->getValue().asReal());
        applySelectedUnitTransform(
            gAgent.getPosGlobalFromAgent(agent_pos), rotation, scale);
    }
}

void ALPanelGhostStudio::onYawCommit()
{
    LLVector3d foot;
    LLQuaternion rotation;
    F32 scale = 1.f;
    bool collapsed_group = false;
    if (selectedUnitTransform(
            foot, rotation, scale, collapsed_group))
    {
        const F32 yaw_degrees = (F32)mYawSpin->getValue().asReal();
        mHeadingDial->setValue(yaw_degrees);
        applySelectedUnitTransform(
            foot,
            LLQuaternion(yaw_degrees * DEG_TO_RAD, LLVector3::z_axis),
            scale);
    }
}

void ALPanelGhostStudio::onHeadingDialCommit()
{
    LLVector3d foot;
    LLQuaternion rotation;
    F32 scale = 1.f;
    bool collapsed_group = false;
    if (selectedUnitTransform(
            foot, rotation, scale, collapsed_group))
    {
        const F32 yaw_degrees = (F32)mHeadingDial->getValue().asReal();
        mYawSpin->setValue(yaw_degrees);
        applySelectedUnitTransform(
            foot,
            LLQuaternion(yaw_degrees * DEG_TO_RAD, LLVector3::z_axis),
            scale);
    }
}

void ALPanelGhostStudio::onLookTargetCommit()
{
    ALGhostStudio::Instance* inst =
        ALGhostStudio::instance().getInstance(selectedInstance());
    if (!inst)
    {
        return;
    }
    const std::string value = mLookTargetCombo->getValue().asString();
    ALGhostStudio::ELookTarget target = ALGhostStudio::LOOK_TARGET_CAMERA;
    LLUUID target_id;
    if (value == "me")
    {
        target = ALGhostStudio::LOOK_TARGET_ME;
    }
    else if (value.rfind("actor:", 0) == 0)
    {
        target = ALGhostStudio::LOOK_TARGET_ACTOR;
        target_id.set(value.substr(6), false);
    }
    else if (value.rfind("ghost:", 0) == 0)
    {
        target = ALGhostStudio::LOOK_TARGET_GHOST;
        target_id.set(value.substr(6), false);
    }
    ALGhostStudio::instance().setLookTarget(
        inst->mId, target, target_id, mKeepFacingCheck->get());
}

void ALPanelGhostStudio::onClickFaceNow()
{
    onLookTargetCommit();
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        ALGhostStudio::instance().faceInstance(inst->mId);
        const F32 degrees = inst->getYaw() * RAD_TO_DEG;
        mHeadingDial->setValue(degrees);
        mYawSpin->setValue(degrees);
    }
}

void ALPanelGhostStudio::onKeepFacingCommit()
{
    onLookTargetCommit();
    if (mKeepFacingCheck->get())
    {
        onClickFaceNow();
    }
}

void ALPanelGhostStudio::onChaosCommit()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID id = selectedInstance();
    if (studio.groupForMember(id))
    {
        return;
    }
    if (ALGhostStudio::Instance* inst = studio.getInstance(id))
    {
        const F32 amount = mChaosCheck->get()
            ? (F32)mChaosSlider->getValue().asReal() : 0.f;
        studio.setInstanceChaos(inst->mId, amount);
        mChaosSlider->setEnabled(mChaosCheck->get());
    }
}

void ALPanelGhostStudio::onScaleCommit()
{
    LLVector3d foot;
    LLQuaternion rotation;
    F32 scale = 1.f;
    bool collapsed_group = false;
    if (selectedUnitTransform(
            foot, rotation, scale, collapsed_group))
    {
        const F32 requested_scale =
            (F32)mScaleSpin->getValue().asReal();
        if (!applySelectedUnitTransform(
                foot, rotation, requested_scale))
        {
            // The model never silently clamps a group factor. Restore the
            // displayed authoritative value if a runtime or bound rejected it.
            mScaleSpin->setValue(scale);
        }
    }
}

void ALPanelGhostStudio::onAnimSpeedCommit()
{
    const F32 speed =
        llclamp((F32)mAnimSpeedSpin->getValue().asReal(), 0.05f, 4.f);
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        studio.setInstanceAnimSpeed(id, speed);
    }
}

void ALPanelGhostStudio::onPhysicsCommit()
{
    const bool enabled = mPhysicsCheck->get();
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        studio.setInstancePhysicsEnabled(id, enabled);
    }
}

void ALPanelGhostStudio::onClickAnimPause()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        studio.setInstancePaused(id, true);
    }
}

void ALPanelGhostStudio::onClickAnimResume()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        studio.setInstancePaused(id, false);
    }
}

void ALPanelGhostStudio::onDriveModeCommit()
{
    // A RIGID group is one authoring unit: a drive-mode change (this is the
    // "Mirror the actor" pose control) must reach every member, not just the
    // list row's representative. selectedInstance() collapses a selected
    // group header down to its FIRST member only, which used to leave every
    // other ghost in the group on its previous drive mode. selectedInstances()
    // expands a selected group header to its full membership -- the same
    // pattern sibling commit handlers (onLoopModeCommit, onStyleCommit, ...)
    // already use.
    const ALGhostStudio::EDriveMode mode =
        (ALGhostStudio::EDriveMode)mDriveModeCombo->getValue().asInteger();
    LLUUID anim(mDirectedAnimEdit->getText());
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            studio.setInstanceDriveMode(id, mode, anim);
        }
    }
}

// [AutoAnimate] All picker replies capture ids only; the panel may have closed.
void ALPanelGhostStudio::autoAnimFileReply(const std::vector<LLUUID>& ids, bool save,
                                         const std::vector<std::string>& filenames)
{
    if (filenames.empty()) return;
    ALGhostStudio& studio = ALGhostStudio::instance();
    U32 index = 0;
    U32 skipped = 0;
    for (const LLUUID& id : ids)
    {
        const auto* inst = studio.getInstance(id);
        if (!inst || inst->mKind != ALGhostStudio::BACKING_ENTITY_CLONE) continue;
        std::string path = filenames.front();
        // A multi-selection saves one file per entity instead of overwriting the first.
        // [FileSafety] Insert "_<8 hex>" into the BASE NAME only (never a parent folder name),
        // and never overwrite an existing sibling: only the exact file the user picked
        // (index 0, already confirmed by the OS save dialog) may be replaced.
        if (save && index != 0)
        {
            const std::string dir = gDirUtilp->getDirName(filenames.front());
            const std::string full_base = gDirUtilp->getBaseFileName(filenames.front(), false);
            const std::string stem = gDirUtilp->getBaseFileName(filenames.front(), true);
            // Preserve the extension's original case; empty when there is none.
            const std::string ext = full_base.size() > stem.size() ? full_base.substr(stem.size()) : std::string();
            const std::string sibling = stem + "_" + id.asString().substr(0, 8) + ext;
            path = dir.empty() ? sibling : gDirUtilp->add(dir, sibling);
            // Atomic create-new: closes the check-then-write race. exists==true means a file
            // (or something else) already occupies the name and was left untouched.
            bool exists = false;
            studio.saveAutoAnimate(id, path, true, &exists);
            if (exists)
            {
                LL_WARNS("GhostStudio") << "AUTOANIM-SAVE skip existing sibling file " << path << LL_ENDL;
                ++skipped;
            }
            ++index;
            continue;
        }
        if (save) studio.saveAutoAnimate(id, path); else studio.loadAutoAnimate(id, path);
        ++index;
    }
    if (skipped > 0)
    {
        LLSD args;
        args["MESSAGE"] = llformat("%u additional AutoAnimate file(s) were NOT saved because a file with the "
                                   "same name already exists next to the file you picked. Existing files were "
                                   "left untouched; pick a different name or folder.", skipped);
        LLNotificationsUtil::add("GenericAlertOK", args);
    }
}
void ALPanelGhostStudio::onAutoAnimFile(bool save)
{
    const auto ids = selectedInstances();
    const auto callback = [ids, save](const std::vector<std::string>& files, LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
    { ALPanelGhostStudio::autoAnimFileReply(ids, save, files); };
    if (save) LLFilePickerReplyThread::startPicker(callback, LLFilePicker::FFSAVE_XML, "ghost_performance.xml");
    else LLFilePickerReplyThread::startPicker(callback, LLFilePicker::FFLOAD_XML, false);
}
void ALPanelGhostStudio::onAutoAnimToggle()
{
    for (const LLUUID& id : selectedInstances()) ALGhostStudio::instance().setInstanceAutoAnimate(id, mAutoAnimCheck->get());
}
void ALPanelGhostStudio::onAutoAnimWindow()
{
    for (const LLUUID& id : selectedInstances()) ALGhostStudio::instance().setInstanceAutoAnimWindow(id, mAutoAnimWindow->getValueF32());
}
void ALPanelGhostStudio::onAutoAnimAction(ALGhostAutoAnim::Action action)
{
    for (const LLUUID& id : selectedInstances()) ALGhostStudio::instance().autoAnimateAction(id, action);
}
void ALPanelGhostStudio::refreshAutoAnimate(const ALGhostStudio::Instance* inst)
{
    const bool entity = inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
    ALGhostAutoAnim::LearnStatus status;
    if (entity) ALGhostStudio::instance().getAutoAnimateStatus(inst->mId, status);
    setToolTipIfChanged(mAutoAnimCheck, std::string(entity ? "Learn the source's idle performance, then continue independently" :
            "AutoAnimate needs an entity clone (overlays redraw the live source)"));
    // [GhostPanelPerf] diff-guarded writes (this runs at the detail tick, not per frame)
    setEnabledIfChanged(mAutoAnimCheck, entity);
    setValueIfChanged(mAutoAnimCheck, LLSD(entity && inst->mAutoAnimate), true);
    setEnabledIfChanged(mAutoAnimWindow, entity);
    setValueIfChanged(mAutoAnimWindow, LLSD((F64)status.mWindow));
    setEnabledIfChanged(mAutoAnimReset, entity);
    setEnabledIfChanged(mAutoAnimStop, entity && status.mLearning);
    setEnabledIfChanged(mAutoAnimRelearn, entity);
    setEnabledIfChanged(mAutoAnimReseed, entity && status.mOn);
    bool entries = status.mLiveEntries != 0;
    if (entity && inst->mAutoAnimPool)
    {
        entries = entries || !inst->mAutoAnimPool->mBody.entries.empty();
        for (const auto& channel : inst->mAutoAnimPool->mAnimesh) entries = entries || !channel.second.entries.empty();
    }
    setEnabledIfChanged(mAutoAnimGo, entity && status.mOn && entries &&
        (inst->mDriveMode == ALGhostStudio::DRIVE_MIRROR || inst->mDriveMode == ALGhostStudio::DRIVE_TRUE_MIRROR));
    setEnabledIfChanged(mAutoAnimFollow, entity && (status.mBodyAutonomous || status.mAnimeshAutonomous));
    setEnabledIfChanged(mAutoAnimSave, entity && bool(inst->mAutoAnimPool));
    setEnabledIfChanged(mAutoAnimLoad, entity);
    // [AutoAnimate] Fractional observed time does not change the visible clock.
    const S32 mode = entity ? static_cast<S32>(inst->mDriveMode) : -1;
    const auto& previous = mAutoAnimDisplay;
    if (mAutoAnimDisplayValid && mode == mAutoAnimDisplayMode &&
        status.mOn == previous.mOn && status.mLearning == previous.mLearning && status.mGated == previous.mGated &&
        static_cast<S32>(status.mObserved) == static_cast<S32>(previous.mObserved) &&
        static_cast<S32>(status.mWindow) == static_cast<S32>(previous.mWindow) &&
        status.mBodyStates == previous.mBodyStates && status.mBodyOneShots == previous.mBodyOneShots &&
        status.mAnimeshChannels == previous.mAnimeshChannels && status.mAnimeshAutonomous == previous.mAnimeshAutonomous &&
        status.mBodyAutonomous == previous.mBodyAutonomous && status.mManual == previous.mManual &&
        status.mDetail == previous.mDetail) return;
    mAutoAnimDisplay = status; mAutoAnimDisplayMode = mode; mAutoAnimDisplayValid = true;
    std::string text = "AutoAnimate off";
    if (status.mOn)
    {
        if (status.mBodyAutonomous || status.mAnimeshAutonomous)
            text = std::string("Autonomous (") + (status.mManual ? "manual" : "automatic") + ") - " +
                (status.mBodyAutonomous ? "body + " : "") + std::to_string(status.mAnimeshAutonomous) + " animesh";
        else if (status.mGated) text = "Paused: source " + std::string(status.mDetail.find("source sit") != std::string::npos ? "sitting" : "moving / airborne");
        else if (status.mLearning)
        {
            const S32 elapsed = static_cast<S32>(status.mObserved), window = static_cast<S32>(status.mWindow);
            text = llformat("Learning %d:%02d / %d:%02d", elapsed / 60, elapsed % 60, window / 60, window % 60);
        }
        else text = llformat("Learned: body %u stands / %u one-shots - %u animesh", status.mBodyStates, status.mBodyOneShots, status.mAnimeshChannels);
        if (inst->mDriveMode == ALGhostStudio::DRIVE_DIRECTED) text += " (dormant: Directed)";
        else if (inst->mDriveMode == ALGhostStudio::DRIVE_FROZEN) text += " (dormant: Frozen)";
    }
    setTextIfChanged(mAutoAnimStatus, text);
    setToolTipIfChanged(mAutoAnimStatus, status.mDetail);
}

void ALPanelGhostStudio::onDirectedAnimCommit()
{
    // See onDriveModeCommit(): apply to every member of a selected group, not
    // just the representative.
    LLUUID anim(mDirectedAnimEdit->getText());
    if (anim.isNull())
    {
        return;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            inst->mDriveMode == ALGhostStudio::DRIVE_DIRECTED)
        {
            studio.setInstanceDriveMode(id, ALGhostStudio::DRIVE_DIRECTED, anim);
        }
    }
}

void ALPanelGhostStudio::onAnimationLibraryCommit()
{
    const LLUUID asset_id =
        mAnimationLibraryCombo->getSelectedValue().asUUID();
    if (asset_id.isNull())
    {
        return;
    }
    mDirectedAnimEdit->setText(asset_id.asString());
    onDirectedAnimCommit();
    ALGhostAnimAssetIndex::instance().requestMetadata(asset_id);
}

void ALPanelGhostStudio::onLoopModeCommit()
{
    const auto mode = (ALGhostStudio::ELoopMode)
        mLoopModeCombo->getValue().asInteger();
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        studio.setInstanceLoopMode(id, mode);
    }
}

void ALPanelGhostStudio::onClickAnimSync()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        studio.restartInstanceAnimation(id);
    }
}

void ALPanelGhostStudio::onLensGazeToggle()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    LLActorMover& mover = LLActorMover::instance();
    const S32 mode = mLensGazeTargetMode->getValue().asInteger();
    for (const LLUUID& id : selectedInstances())
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            inst->mEntityId.notNull())
        {
            mover.setGazeTargetMode(inst->mEntityId, mode);
            mover.setGazeEnabled(inst->mEntityId, mLensGazeCheck->get());
        }
    }
}

void ALPanelGhostStudio::onClickLensGazeSelection()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    LLActorMover& mover = LLActorMover::instance();
    for (const LLUUID& id : selectedInstances())
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (!inst || inst->mKind != ALGhostStudio::BACKING_ENTITY_CLONE ||
            inst->mEntityId.isNull())
        {
            continue;
        }
        mover.setGazeTargetMode(inst->mEntityId,
            mLensGazeTargetMode->getValue().asInteger());
        mover.setGazeEnabled(inst->mEntityId, true);
    }
}

void ALPanelGhostStudio::onLensGazeSettingsCommit()
{
    mDetailDirty = true; mCrowdDirty = true; // [GhostPanelPerf] enables depend on this widget's value
    ALGhostStudio& studio = ALGhostStudio::instance();
    LLActorMover& mover = LLActorMover::instance();
    const S32 mode = llclamp(
        mLensGazeTargetMode->getValue().asInteger(),
        (S32)LLActorMover::GAZE_TANGENT, (S32)LLActorMover::GAZE_POINT);
    const LLUUID cast_target =
        mLensGazeCastTarget->getSelectedValue().asUUID();
    const F32 head_eye = llclamp(
        (F32)mLensGazeHeadEyeSlider->getValue().asReal(), 0.f, 1.f);
    const F32 intensity = llclamp(
        (F32)mLensGazeIntensitySlider->getValue().asReal(), 0.f, 1.f);
    const F32 smoothing = llclamp(
        (F32)mLensGazeSmoothingSlider->getValue().asReal(), 0.f, 1.f);
    for (const LLUUID& id : selectedInstances())
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (!inst || inst->mKind != ALGhostStudio::BACKING_ENTITY_CLONE ||
            inst->mEntityId.isNull())
        {
            continue;
        }
        mover.setGazeTargetMode(inst->mEntityId, mode);
        mover.setGazeCastTarget(inst->mEntityId, cast_target);
        mover.setGazeHeadEyeBlend(inst->mEntityId, head_eye);
        mover.setGazeIntensity(inst->mEntityId, intensity);
        mover.setGazeSmoothing(inst->mEntityId, smoothing);
    }
}

void ALPanelGhostStudio::onLensGazeTorsoCommit()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    LLActorMover& mover = LLActorMover::instance();
    const F32 amount = llclamp(
        (F32)mLensGazeTorsoSlider->getValue().asReal(), 0.f, 1.f);
    for (const LLUUID& id : selectedInstances())
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            inst->mEntityId.notNull())
        {
            mover.setGazeTorsoAmount(inst->mEntityId, amount);
        }
    }
}

void ALPanelGhostStudio::onClickPlace()
{
    const LLUUID sel = selectedUnitId();
    if (sel.isNull())
    {
        return;
    }
    // one-shot: the tool places on the next ground click and hands the camera
    // straight back (Esc / right-click cancels) -- ALToolPathEdit's walk-to idiom
    ALToolGhostPlace* tool = ALToolGhostPlace::getInstance();
    exitPlaceMode();
    exitEditMode();
    if (!tool->armFor(mPlacementOwner, sel))
    {
        return;
    }
    LLToolMgr::getInstance()->setTransientTool(tool);
}

void ALPanelGhostStudio::onClickToActor()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID unit_id = selectedUnitId();
    ALGhostStudio::Instance* inst =
        studio.getInstance(selectedInstance());
    if (!inst)
    {
        return;
    }
    LLVOAvatar* av = LLDirectorCast::instance().resolve(inst->mSource);
    if (av && av->getRootJoint())
    {
        LLVector3 foot = av->getBaseRootPosition();   // [ClientHeight] Ghost placements are BASE space
        F32 pelvis_to_foot = av->getPelvisToFoot();
        if (!llfinite(pelvis_to_foot))
        {
            pelvis_to_foot = 0.f;
        }
        foot.mV[VZ] -= llmax(0.f, pelvis_to_foot);
        if (!foot.isFinite())
        {
            return;
        }
        LLVector3d unit_foot;
        LLQuaternion unit_rotation;
        F32 unit_scale = 1.f;
        if (studio.getUnitTransform(
                unit_id, unit_foot, unit_rotation, unit_scale))
        {
            applyUnitTransform(
                unit_id, gAgent.getPosGlobalFromAgent(foot),
                unit_rotation, unit_scale);
        }
    }
}

void ALPanelGhostStudio::onClickToMe()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID unit_id = selectedUnitId();
    ALGhostStudio::Instance* inst =
        studio.getInstance(selectedInstance());
    if (!inst || !isAgentAvatarValid() || !gAgentAvatarp->getRootJoint())
    {
        return;
    }
    LLVector3 foot = gAgentAvatarp->getBaseRootPosition();   // [ClientHeight] Ghost placements are BASE space
    F32 pelvis_to_foot = gAgentAvatarp->getPelvisToFoot();
    if (!llfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    foot.mV[VZ] -= llmax(0.f, pelvis_to_foot);
    if (!foot.isFinite())
    {
        return;
    }
    LLVector3d unit_foot;
    LLQuaternion unit_rotation;
    F32 unit_scale = 1.f;
    if (studio.getUnitTransform(
            unit_id, unit_foot, unit_rotation, unit_scale))
    {
        applyUnitTransform(
            unit_id, gAgent.getPosGlobalFromAgent(foot),
            unit_rotation, unit_scale);
    }
}

void ALPanelGhostStudio::onClickDrop()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedUnitIds())
    {
        LLVector3d foot;
        LLQuaternion rotation;
        F32 scale = 1.f;
        if (!studio.getUnitTransform(id, foot, rotation, scale)) continue;
        foot.mdV[VZ] = LLWorld::getInstance()->resolveLandHeightGlobal(foot);
        applyUnitTransform(id, foot, rotation, scale);
    }
}

void ALPanelGhostStudio::onClickAlignFeet()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    LLVector3d anchor_foot;
    LLQuaternion anchor_rotation;
    F32 anchor_scale = 1.f;
    if (!studio.getUnitTransform(
            selectedUnitId(), anchor_foot,
            anchor_rotation, anchor_scale)) return;
    const F64 z = anchor_foot.mdV[VZ];
    for (const LLUUID& id : selectedUnitIds())
    {
        LLVector3d foot;
        LLQuaternion rotation;
        F32 scale = 1.f;
        if (!studio.getUnitTransform(id, foot, rotation, scale)) continue;
        foot.mdV[VZ] = z;
        applyUnitTransform(id, foot, rotation, scale);
    }
}

void ALPanelGhostStudio::onClickUpright()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedUnitIds())
    {
        LLVector3d foot;
        LLQuaternion rotation;
        F32 scale = 1.f;
        if (!studio.getUnitTransform(id, foot, rotation, scale)) continue;
        const LLVector3 forward = LLVector3::x_axis * rotation;
        const F32 yaw = atan2f(
            forward.mV[VY], forward.mV[VX]);
        applyUnitTransform(
            id, foot, LLQuaternion(yaw, LLVector3::z_axis), scale);
    }
}

void ALPanelGhostStudio::onClickCopyTransform()
{
    if (ALGhostStudio::instance().getUnitTransform(
            selectedUnitId(), mClipboardFoot,
            mClipboardRotation, mClipboardScale))
    {
        mHasTransformClipboard = true;
    }
}

void ALPanelGhostStudio::onClickPasteTransform()
{
    if (!mHasTransformClipboard) return;
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedUnitIds())
    {
        applyUnitTransform(
            id, mClipboardFoot, mClipboardRotation,
            mClipboardScale);
    }
}

void ALPanelGhostStudio::exitPlaceMode()
{
    if (ALToolGhostPlace::instanceExists())
    {
        ALToolGhostPlace::getInstance()->cancelForOwner(mPlacementOwner);
    }
}

// ---------------------------------------------------------------------------
// [R2-3] in-world edit mode (persistent ALToolGhostEdit)
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onToggleEditMode()
{
    const bool want = mEditModeCheck->get();
    if (want)
    {
        const ALGhostStudio::CrowdPlacementDraft& draft =
            ALGhostStudio::instance().getCrowdPlacementDraft();
        if (draft.mActive)
        {
            mEditMode = false;
            mEditModeCheck->set(false);
            return;
        }
        // the one-shot Place arm and the persistent edit mode are exclusive
        exitPlaceMode();
        ALToolGhostEdit* tool = ALToolGhostEdit::getInstance();
        if (!tool->beginEditFor(mPlacementOwner))
        {
            mEditMode = false;
            mEditModeCheck->set(false);
            return;
        }
        LLToolMgr::getInstance()->setTransientTool(tool);
        mEditMode =
            tool->isEditModeActive() &&
            tool->editOwner() == mPlacementOwner;
        if (!mEditMode)
        {
            tool->stopEditModeFor(mPlacementOwner);
            mEditModeCheck->set(false);
        }
    }
    else
    {
        exitEditMode();
    }
}

void ALPanelGhostStudio::exitEditMode()
{
    ALToolGhostEdit* tool = ALToolGhostEdit::getInstance();
    const bool owns_edit = tool->editOwner() == mPlacementOwner;
    mEditMode = false;
    if (owns_edit)
    {
        // The proxy may have handed off to the stock toolset, so ALToolGhostEdit
        // is no longer current/transient -- stopEditMode() tears the proxy down
        // and restores the prior toolset regardless. Also clear the transient
        // tool in the (no-selection) case where the ghost tool is still current.
        tool->stopEditModeFor(mPlacementOwner);
        LLToolMgr* tm = LLToolMgr::getInstance();
        if (tm->usingTransientTool()
            && tm->getCurrentTool() == tool)
        {
            tm->clearTransientTool();   // restores the prior tool (camera control)
        }
    }
    if (mEditModeCheck)
    {
        mEditModeCheck->set(false);
    }
}

// ---------------------------------------------------------------------------
// look
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onStyleCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
        {
            inst->mStyle = mStyleCombo->getValue().asInteger();
            ALGhostStudio::instance().touchInstances(); // [GhostPanelPerf] Style column
        }
    }
}

void ALPanelGhostStudio::onActorTintToggle()
{
    mDetailDirty = true; mCrowdDirty = true; // [GhostPanelPerf] enables depend on this widget's value
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mUseActorTint = mActorTintCheck->get();
    }
}

void ALPanelGhostStudio::onHueCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mHue = (F32)mHueSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onAlphaCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mAlpha = llclamp((F32)mAlphaSlider->getValue().asReal(), 0.f, 1.f);
    }
}

void ALPanelGhostStudio::onShimmerSpeedCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mShimmerSpeed = (F32)mShimmerSpeedSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onShimmerAmountCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mShimmerIntensity = (F32)mShimmerAmountSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onPixelCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mPixelSize = (F32)mPixelSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onGlitchCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mGlitch = (F32)mGlitchSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onDistortCommit()
{
    mDetailDirty = true; mCrowdDirty = true; // [GhostPanelPerf] enables depend on this widget's value
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mDistort = mDistortCombo->getValue().asInteger();
    }
    mDistortAmountSlider->setEnabled(
        mDistortCombo->getValue().asInteger() != 0);
}

void ALPanelGhostStudio::onDistortAmountCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mDistortAmount =
                llclamp((F32)mDistortAmountSlider->getValue().asReal(), 0.f, 1.f);
    }
}

// [R2-1] output brightness -- the unlit clone's night-scene dimmer. A
// "match scene" helper was considered and SKIPPED: the ghost draws into the
// post-tonemap overlay, and a sky ambient/sun probe yields scene-referred
// linear values that do not map to a post-tonemap multiplier without
// inverting exposure + tonemap and ignoring local lights -- the estimate
// would be least reliable exactly in the night-with-practicals shots it is
// meant for. The slider is the honest control.
void ALPanelGhostStudio::onBrightnessCommit()
{
    for (const LLUUID& id : selectedInstances())
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
            inst->mBrightness = llclamp(
                (F32)mBrightnessSlider->getValue().asReal(), 0.05f, 1.5f);
    }
}

void ALPanelGhostStudio::onEffectFpsCommit()
{
    for (const LLUUID& id : selectedInstances())
        ALGhostStudio::instance().setEffectFps(
            id, (F32)mEffectFps->getValue().asReal());
}

void ALPanelGhostStudio::onEntityLookCommit()
{
    // See onDriveModeCommit(): apply to every member of a selected group, not
    // just the representative selectedInstance() would collapse to.
    const ALGhostStudio::EGhostLook look =
        (ALGhostStudio::EGhostLook)mEntityLookCombo->getValue().asInteger();
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : selectedInstances())
    {
        studio.setInstanceLook(id, look);
    }
}

// ---------------------------------------------------------------------------
// pose
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onClickFreeze()
{
    const std::vector<LLUUID> ids = selectedInstances();
    if (ids.empty())
    {
        return;
    }
    bool any_failed = false;
    for (const LLUUID& id : ids)
        any_failed = !ALGhostStudio::instance().freezeInstance(id) || any_failed;
    if (any_failed)
    {
        // most common miss: the pipeline is not rendering the source this
        // frame (instance disabled / master off / skins still loading)
        mPoseStatus->setText(std::string("Nothing to grab yet -- ghost must be rendering"));
    }
}

void ALPanelGhostStudio::onClickLive()
{
    for (const LLUUID& id : selectedInstances())
        ALGhostStudio::instance().unfreezeInstance(id);
}

// ---------------------------------------------------------------------------
// array helper + master toggle
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onClickArray(ALGhostStudio::EFormation formation)
{
    // Quick buttons are presets, not a second placement path.
    mFormationCombo->setValue((S32)formation);
    onFormationCommit();
    onClickBuildArray();
}

void ALPanelGhostStudio::onClickBuildArray()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::CrowdPlacementDraft& current =
        studio.getCrowdPlacementDraft();
    if (current.mActive && current.mOwnerId != mPlacementOwner)
    {
        mLastCrowdMessage =
            "Another Ghost Studio panel owns the active placement.";
        return;
    }
    if (current.mActive)
    {
        return;
    }

    LLUUID prototype_id = selectedInstance();
    if (prototype_id.isNull())
    {
        prototype_id = studio.getSelected();
    }
    const ALGhostStudio::Instance* prototype =
        studio.getInstance(prototype_id);
    if (!prototype)
    {
        mLastCrowdMessage = "Select a clone to use as the prototype.";
        return;
    }
    if (prototype->mGroupId.notNull())
    {
        mLastCrowdMessage =
            "Ungroup this crowd before using one member as a prototype.";
        refreshCrowdPlacement();
        return;
    }

    exitPlaceMode();
    exitEditMode();
    if (ALToolGhostEdit::instanceExists() &&
        ALToolGhostEdit::getInstance()->isEditModeActive())
    {
        mLastCrowdMessage =
            "Finish the other Ghost Studio panel's edit session first.";
        return;
    }
    if (!studio.beginCrowdPlacement(
            mPlacementOwner, prototype_id, capturePlacementSpec(),
            prototype->mFootGlobal, prototype->getYaw(), 0.f))
    {
        mLastCrowdMessage = "Could not start crowd placement.";
        return;
    }
    // A facing-offset value set BEFORE "Preview in world" (no active draft
    // existed yet, so the spinner's own commit callback was a no-op) must
    // not be silently discarded: seed the just-created draft with whatever
    // the spinner currently shows before refreshCrowdPlacement() below can
    // pull the display back down to the draft's still-default 0.
    studio.setCrowdPlacementFacingOffset(mPlacementOwner,
        (F32)mFacingYawOffset->getValue().asReal() * DEG_TO_RAD);

    const ALGhostStudio::CrowdPlacementDraft& draft =
        studio.getCrowdPlacementDraft();
    const bool draft_can_commit = draft.mValidation.mCanCommit;
    const std::string draft_message = draft.mValidation.mMessage;
    std::string source_message;
    const bool source_unchanged =
        studio.crowdPlacementSourceUnchanged(&source_message);
    if (!ALToolCrowdPlace::getInstance()->armFor(
            mPlacementOwner, draft.mSessionId))
    {
        const std::string tool_message =
            ALToolCrowdPlace::getInstance()->lastMessage();
        studio.cancelCrowdPlacement(mPlacementOwner);
        if (!source_unchanged && !source_message.empty())
        {
            mLastCrowdMessage = source_message;
        }
        else if (!draft_can_commit && !draft_message.empty())
        {
            mLastCrowdMessage = draft_message;
        }
        else if (!tool_message.empty())
        {
            mLastCrowdMessage = tool_message;
        }
        else
        {
            mLastCrowdMessage =
                "The placement tool could not take control.";
        }
        refreshCrowdPlacement();
        return;
    }
    mLastCrowdMessage.clear();
    refreshCrowdPlacement();
}

void ALPanelGhostStudio::captureFormationParameter()
{
    const CrowdParameterDescriptor descriptor =
        crowd_parameter_descriptor(mConfiguredFormation);
    const F32 value = (F32)mFormationParam->getValue().asReal();
    switch (descriptor.mParameter)
    {
    case ECrowdParameter::GRID_COLUMNS:
        mFormationGridColumns = (U32)ll_round(value);
        break;
    case ECrowdParameter::ARC_SWEEP:
        mFormationArcSweep = value;
        break;
    case ECrowdParameter::LANE_GAP:
        mFormationLaneGap = value;
        break;
    case ECrowdParameter::CHEVRON_ANGLE:
        mFormationChevronAngle = value;
        break;
    case ECrowdParameter::ZIGZAG_COLUMNS:
        mFormationZigzagColumns = (U32)ll_round(value);
        break;
    case ECrowdParameter::HORSESHOE_OPENING:
        mFormationHorseshoeOpening = value;
        break;
    case ECrowdParameter::SPIRAL_TURNS:
        mFormationSpiralTurns = value;
        break;
    case ECrowdParameter::ASPECT_RATIO:
        mFormationAspectRatio = value;
        break;
    case ECrowdParameter::RAY_COUNT:
        mFormationRayCount = (U32)ll_round(value);
        break;
    case ECrowdParameter::STAGGER_OFFSET:
        mFormationStaggerOffset = value;
        break;
    case ECrowdParameter::NONE:
        break;
    }
}

void ALPanelGhostStudio::configureFormationParameter(
    ALGhostStudio::EFormation formation)
{
    const CrowdParameterDescriptor descriptor =
        crowd_parameter_descriptor(formation);
    const bool visible = descriptor.mParameter != ECrowdParameter::NONE;

    // [GhostPanelPerf] Deferral comes FIRST, before ANY visibility or configuration
    // change: hiding or reconfiguring the spinner (setPrecision rewrites its inline
    // editor) while the operator is editing it would strand or lose the edit. The
    // pending flag is retried from refreshCrowdPlacement() once editing ends.
    // mConfiguredFormation is what the spinner is ACTUALLY configured for (its commit
    // handler stores the value under it), so it only changes once the guard passes; the
    // requested formation waits in mPendingFormation meanwhile.
    if (alIsBeingEdited(mFormationParam))
    {
        mPendingFormation = formation;
        mParamConfigPending = true;
        return;
    }
    mConfiguredFormation = formation;
    mParamConfigPending = false;
    setVisibleIfChanged(mFormationParam, visible);
    setVisibleIfChanged(mFormationParamReset, visible);
    if (!visible)
    {
        mParamConfigShown = -1;
        return;
    }

    // The descriptor configuration (label, tooltip, range, increment, precision) is
    // cached by FORMATION id -- two formations can share a parameter kind yet carry a
    // different label/tooltip (STAGGERED_MILITARY vs RING_BRIGADE, Soul Train variants).
    if (mParamConfigShown != (S32)formation)
    {
        mFormationParam->setLabel(std::string(descriptor.mLabel));
        setToolTipIfChanged(mFormationParam, std::string(descriptor.mTooltip));
        setToolTipIfChanged(mFormationParamReset,
            std::string("Reset ") + descriptor.mLabel + " for this formation");
        // shrinking range: max before value (the value write is below)
        mFormationParam->setMinValue(descriptor.mMinimum);
        setMaxValueIfChanged(mFormationParam, descriptor.mMaximum);
        mFormationParam->setIncrement(descriptor.mIncrement);
        mFormationParam->setPrecision(descriptor.mPrecision);
        mParamConfigShown = (S32)formation;
    }
    F32 value = descriptor.mDefault;
    switch (descriptor.mParameter)
    {
    case ECrowdParameter::GRID_COLUMNS:
        value = (F32)mFormationGridColumns;
        break;
    case ECrowdParameter::ARC_SWEEP:
        value = mFormationArcSweep;
        break;
    case ECrowdParameter::LANE_GAP:
        value = mFormationLaneGap;
        break;
    case ECrowdParameter::CHEVRON_ANGLE:
        value = mFormationChevronAngle;
        break;
    case ECrowdParameter::ZIGZAG_COLUMNS:
        value = (F32)mFormationZigzagColumns;
        break;
    case ECrowdParameter::HORSESHOE_OPENING:
        value = mFormationHorseshoeOpening;
        break;
    case ECrowdParameter::SPIRAL_TURNS:
        value = mFormationSpiralTurns;
        break;
    case ECrowdParameter::ASPECT_RATIO:
        value = mFormationAspectRatio;
        break;
    case ECrowdParameter::RAY_COUNT:
        value = (F32)mFormationRayCount;
        break;
    case ECrowdParameter::STAGGER_OFFSET:
        value = mFormationStaggerOffset;
        break;
    case ECrowdParameter::NONE:
        break;
    }
    // while a draft import is applying, never overwrite a parameter the operator is editing
    setValueIfChanged(mFormationParam, LLSD((F64)value), !mApplyingCrowdDraft);
}

void ALPanelGhostStudio::importFormationParameters(
    const ALGhostStudio::FormationSpec& spec)
{
    switch (crowd_parameter_descriptor(spec.mFormation).mParameter)
    {
    case ECrowdParameter::GRID_COLUMNS:
        mFormationGridColumns = spec.mGridColumns;
        break;
    case ECrowdParameter::ARC_SWEEP:
        mFormationArcSweep = spec.mArcSweepDegrees;
        break;
    case ECrowdParameter::LANE_GAP:
        mFormationLaneGap = spec.mLaneGap;
        break;
    case ECrowdParameter::CHEVRON_ANGLE:
        mFormationChevronAngle = spec.mChevronAngleDegrees;
        break;
    case ECrowdParameter::ZIGZAG_COLUMNS:
        mFormationZigzagColumns = spec.mZigzagColumns;
        break;
    case ECrowdParameter::HORSESHOE_OPENING:
        mFormationHorseshoeOpening = spec.mHorseshoeOpeningDegrees;
        break;
    case ECrowdParameter::SPIRAL_TURNS:
        mFormationSpiralTurns = spec.mSpiralTurns;
        break;
    case ECrowdParameter::ASPECT_RATIO:
        mFormationAspectRatio = spec.mAspectRatio;
        break;
    case ECrowdParameter::RAY_COUNT:
        mFormationRayCount = spec.mRayCount;
        break;
    case ECrowdParameter::STAGGER_OFFSET:
        mFormationStaggerOffset = spec.mRankOffsetFraction;
        break;
    case ECrowdParameter::NONE:
        break;
    }
}

bool ALPanelGhostStudio::formationSupportsFreezeStrip(
    ALGhostStudio::EFormation formation) const
{
    switch (formation)
    {
    case ALGhostStudio::FORMATION_LINE:
    case ALGhostStudio::FORMATION_GRID:
    case ALGhostStudio::FORMATION_STAGGERED_ROWS:
    case ALGhostStudio::FORMATION_RING:
    case ALGhostStudio::FORMATION_ARC:
    case ALGhostStudio::FORMATION_CONCENTRIC_RINGS:
    case ALGhostStudio::FORMATION_SPIRAL:
    case ALGhostStudio::FORMATION_SCATTER:
        return true;
    default:
        return false;
    }
}

ALGhostStudio::FormationSpec ALPanelGhostStudio::capturePlacementSpec() const
{
    ALGhostStudio::FormationSpec spec;
    spec.mFormation = (ALGhostStudio::EFormation)
        mFormationCombo->getValue().asInteger();
    // The guide is intentionally never a fit boundary. Every preview solves
    // the full requested count; guide shape/size are presentation-only.
    spec.mEnvelopePolicy = ALFormationSolver::EnvelopePolicy::FitCount;
    spec.mCount = mArrayCount->getValue().asInteger();
    spec.mCenterSpacing = (F32)mArraySpacing->getValue().asReal();
    spec.mEdgeGap = (F32)mFormationEdgeGap->getValue().asReal();
    spec.mJitter = (F32)mFormationJitter->getValue().asReal();
    spec.mGuideShape = (ALGhostStudio::EGuideShape)
        mFormationGuideShape->getValue().asInteger();
    spec.mGuideSize =
        (F32)mFormationGuideSize->getValue().asReal();
    if (formation_uses_grid(spec.mFormation))
    {
        spec.mGridColumns = mBrigadeColumns->getValue().asInteger();
        spec.mGridRows = mBrigadeRows->getValue().asInteger();
        // The corridor mirrors the ROWS x COLUMNS block on each side of the
        // aisle, so its total clone count is twice one block.
        const S32 per_block = (S32)(spec.mGridColumns * spec.mGridRows);
        const S32 blocks =
            spec.mFormation == ALGhostStudio::FORMATION_SOUL_TRAIN_MILITARY
                ? 2 : 1;
        spec.mCount = llclamp(per_block * blocks, 2,
            (S32)ALFormationSolver::MAX_MEMBER_COUNT);
        spec.mFileSpacing =
            (F32)mBrigadeFileSpacing->getValue().asReal();
        spec.mRankSpacing =
            (F32)mBrigadeRankSpacing->getValue().asReal();
    }

    // Only the active formation parameter leaves its canonical typed cache.
    // Every inactive field retains FormationSpec's declared default, so a
    // hidden control can neither invalidate nor perturb this request.
    switch (crowd_parameter_descriptor(spec.mFormation).mParameter)
    {
    case ECrowdParameter::GRID_COLUMNS:
        spec.mGridColumns = mFormationGridColumns;
        break;
    case ECrowdParameter::ARC_SWEEP:
        spec.mArcSweepDegrees = mFormationArcSweep;
        break;
    case ECrowdParameter::LANE_GAP:
        spec.mLaneGap = mFormationLaneGap;
        break;
    case ECrowdParameter::CHEVRON_ANGLE:
        spec.mChevronAngleDegrees = mFormationChevronAngle;
        break;
    case ECrowdParameter::ZIGZAG_COLUMNS:
        spec.mZigzagColumns = mFormationZigzagColumns;
        break;
    case ECrowdParameter::HORSESHOE_OPENING:
        spec.mHorseshoeOpeningDegrees = mFormationHorseshoeOpening;
        break;
    case ECrowdParameter::SPIRAL_TURNS:
        spec.mSpiralTurns = mFormationSpiralTurns;
        break;
    case ECrowdParameter::ASPECT_RATIO:
        spec.mAspectRatio = mFormationAspectRatio;
        break;
    case ECrowdParameter::RAY_COUNT:
        spec.mRayCount = mFormationRayCount;
        break;
    case ECrowdParameter::STAGGER_OFFSET:
        spec.mRankOffsetFraction = mFormationStaggerOffset;
        break;
    case ECrowdParameter::NONE:
        break;
    }
    spec.mSeed = (U32)mFormationSeed->getValue().asInteger();
    spec.mTerrainConform = mFormationTerrain->get();
    spec.mFacing = (ALGhostStudio::EFormationFacing)
        mFormationFacing->getValue().asInteger();
    spec.mPreserveSourceTilt = mPreserveSourceTilt->get();
    spec.mLockMode =
        (ALGhostStudio::ELockMode)mCrowdCreateMode->getValue().asInteger();

    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::CrowdPlacementDraft& draft =
        studio.getCrowdPlacementDraft();
    LLUUID prototype_id =
        draft.mActive && draft.mOwnerId == mPlacementOwner
            ? draft.mSource.mInstanceId
            : selectedInstance();
    if (prototype_id.isNull())
    {
        prototype_id = studio.getSelected();
    }
    const ALGhostStudio::Instance* prototype =
        studio.getInstance(prototype_id);
    if (spec.mFacing == ALGhostStudio::FACING_WORLD_POINT &&
        mCrowdFacingPointValid &&
        mCrowdFacingPrototype == prototype_id &&
        mCrowdFacingPointGlobal.isFinite())
    {
        spec.mHasFacingPoint = true;
        spec.mFacingPointGlobal = mCrowdFacingPointGlobal;
    }
    else if (prototype &&
             (spec.mFacing == ALGhostStudio::FACING_TARGET_ACTOR ||
              spec.mFacing == ALGhostStudio::FACING_TRACK_SUBJECT))
    {
        LLVector3d target;
        if (studio.resolveTargetGlobal(
                *prototype, prototype->mLookTarget,
                prototype->mLookTargetId, prototype->mLookPointGlobal,
                target))
        {
            spec.mHasFacingPoint = true;
            spec.mFacingPointGlobal = target;
        }
    }
    return spec;
}

void ALPanelGhostStudio::updateOwnedCrowdPlacement()
{
    mCrowdDirty = true; // [GhostPanelPerf]
    if (mApplyingCrowdDraft)
    {
        return;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::CrowdPlacementDraft& draft =
        studio.getCrowdPlacementDraft();
    if (draft.mActive && draft.mOwnerId == mPlacementOwner)
    {
        studio.updateCrowdPlacementSpec(
            mPlacementOwner, capturePlacementSpec());
    }
}

void ALPanelGhostStudio::refreshCrowdPlacement()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::CrowdPlacementDraft& draft =
        studio.getCrowdPlacementDraft();
    const bool owner = draft.mActive && draft.mOwnerId == mPlacementOwner;
    const bool blocked = draft.mActive && !owner;
    if (owner)
    {
        mSawOwnedCrowdDraft = true;
    }
    else if (!draft.mActive && mSawOwnedCrowdDraft)
    {
        const std::string& tool_message =
            ALToolCrowdPlace::getInstance()->lastMessage();
        if (!tool_message.empty())
        {
            mLastCrowdMessage = tool_message;
        }
        mSawOwnedCrowdDraft = false;
    }

    if (mParamConfigPending && !alIsBeingEdited(mFormationParam))
    {
        configureFormationParameter(mPendingFormation); // replay the formation deferred while it was edited
    }

    // [GhostPanelPerf] The import goes through the focus-guarded diff helpers (a
    // control the operator is editing is left alone, descendants included). If ANY
    // import control was being edited, the draft revision is NOT accepted: the import
    // stays pending and is retried on the 10 Hz tick until every control has synced.
    if (draft.mActive && draft.mRevision != mLastCrowdDraftRevision &&
        (!mCrowdImportPending || mReadoutTick))
    {
        mApplyingCrowdDraft = true;
        importFormationParameters(draft.mSpec);
        setValueIfChanged(mFormationCombo, LLSD((S32)draft.mSpec.mFormation));
        configureFormationParameter(draft.mSpec.mFormation);
        setValueIfChanged(mArrayCount, LLSD((S32)draft.mSpec.mCount));
        setValueIfChanged(mArraySpacing, LLSD((F64)draft.mSpec.mCenterSpacing));
        setValueIfChanged(mFormationEdgeGap, LLSD((F64)draft.mSpec.mEdgeGap));
        setValueIfChanged(mBrigadeColumns, LLSD((S32)draft.mSpec.mGridColumns));
        setValueIfChanged(mBrigadeRows, LLSD((S32)draft.mSpec.mGridRows));
        setValueIfChanged(mBrigadeFileSpacing, LLSD((F64)draft.mSpec.mFileSpacing));
        setValueIfChanged(mBrigadeRankSpacing, LLSD((F64)draft.mSpec.mRankSpacing));
        setValueIfChanged(mFormationJitter, LLSD((F64)draft.mSpec.mJitter));
        setValueIfChanged(mFormationGuideShape, LLSD((S32)draft.mSpec.mGuideShape));
        setValueIfChanged(mFormationGuideSize, LLSD((F64)draft.mSpec.mGuideSize));
        setValueIfChanged(mFormationSeed, LLSD((S32)draft.mSpec.mSeed));
        setValueIfChanged(mFormationTerrain, LLSD(draft.mSpec.mTerrainConform), true);
        setValueIfChanged(mFormationFacing, LLSD((S32)draft.mSpec.mFacing));
        setValueIfChanged(mPreserveSourceTilt, LLSD(draft.mSpec.mPreserveSourceTilt), true);
        setValueIfChanged(mFacingYawOffset, LLSD((F64)(draft.mFacingYawOffset * RAD_TO_DEG)));
        setValueIfChanged(mCrowdCreateMode, LLSD((S32)draft.mSpec.mLockMode));
        setValueIfChanged(mCrowdCopiesAs, LLSD((S32)draft.mSource.mKind));
        mApplyingCrowdDraft = false;
        mCrowdImportPending =
            alIsBeingEdited(mFormationCombo) || alIsBeingEdited(mFormationParam) ||
            alIsBeingEdited(mArrayCount) || alIsBeingEdited(mArraySpacing) ||
            alIsBeingEdited(mFormationEdgeGap) || alIsBeingEdited(mBrigadeColumns) ||
            alIsBeingEdited(mBrigadeRows) || alIsBeingEdited(mBrigadeFileSpacing) ||
            alIsBeingEdited(mBrigadeRankSpacing) || alIsBeingEdited(mFormationJitter) ||
            alIsBeingEdited(mFormationGuideShape) || alIsBeingEdited(mFormationGuideSize) ||
            alIsBeingEdited(mFormationSeed) || alIsBeingEdited(mFormationFacing) ||
            alIsBeingEdited(mFacingYawOffset) || alIsBeingEdited(mCrowdCreateMode);
        if (!mCrowdImportPending)
        {
            mLastCrowdDraftRevision = draft.mRevision;
        }
    }    else if (!draft.mActive)
    {
        mCrowdImportPending = false;
        mLastCrowdDraftRevision = 0;
    }

    // [GhostPanelPerf] GATE: the ~30 enable/visible writes and the status string run
    // only when the draft revision / ownership / selection / last message changed,
    // when a handler asked (mCrowdDirty), or on the 10 Hz readout tick (which also
    // covers the widget-derived inputs: formation + guide-shape combo values and the
    // owner-only live source-unchanged check).
    const U64 crowd_flags =
        (owner ? 1ULL : 0ULL) | (blocked ? 2ULL : 0ULL) |
        (draft.mActive ? 4ULL : 0ULL) | (draft.mPinned ? 8ULL : 0ULL) |
        (mActiveStrip != 0 ? 16ULL : 0ULL);
    const bool crowd_key_changed = mCrowdGate.changed(
        { draft.mRevision, crowd_flags, selection().mStamp,
          (U64)std::hash<std::string>{}(mLastCrowdMessage) },
        ghost_refresh_always());
    if (!crowd_key_changed && !mReadoutTick && !mCrowdDirty)
    {
        return;
    }
    mCrowdDirty = false;
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ghost refreshCrowdPlacement apply");

    const ALGhostStudio::EFormation formation =
        (ALGhostStudio::EFormation)mFormationCombo->getValue().asInteger();
    const bool has_parameter =
        crowd_parameter_descriptor(formation).mParameter !=
            ECrowdParameter::NONE;
    const bool guide_visible =
        mFormationGuideShape->getValue().asInteger() !=
            (S32)ALGhostStudio::GUIDE_OFF;
    std::string source_reason;
    const bool source_unchanged =
        !owner || studio.crowdPlacementSourceUnchanged(&source_reason);
    const bool can_edit = !blocked;
    const ALGhostStudio::Instance* prototype =
        studio.getInstance(selection().mInstance);
    const bool have_prototype = prototype != nullptr;

    for (LLUICtrl* control :
         { static_cast<LLUICtrl*>(mArrayCount),
           static_cast<LLUICtrl*>(mArraySpacing),
           static_cast<LLUICtrl*>(mFormationCombo),
           static_cast<LLUICtrl*>(mFormationGuideShape),
           static_cast<LLUICtrl*>(mFormationEdgeGap),
           static_cast<LLUICtrl*>(mFormationJitter),
           static_cast<LLUICtrl*>(mFormationSeed),
           static_cast<LLUICtrl*>(mFormationFacing),
           static_cast<LLUICtrl*>(mFormationTerrain),
           static_cast<LLUICtrl*>(mFacingYawOffset),
           static_cast<LLUICtrl*>(mPreserveSourceTilt),
           static_cast<LLUICtrl*>(mCrowdCreateMode) })
    {
        setEnabledIfChanged(control, can_edit);
    }
    const bool uses_grid = formation_uses_grid(formation);
    setVisibleIfChanged(mBrigadeColumns, uses_grid);
    setVisibleIfChanged(mBrigadeRows, uses_grid);
    setVisibleIfChanged(mBrigadeFileSpacing, uses_grid);
    setVisibleIfChanged(mBrigadeRankSpacing, uses_grid);
    setEnabledIfChanged(mArrayCount, can_edit && !uses_grid);
    setVisibleIfChanged(mFormationEdgeGap, !uses_grid);
    setVisibleIfChanged(mFormationJitter, !uses_grid);
    setEnabledIfChanged(mBrigadeColumns, can_edit && uses_grid);
    setEnabledIfChanged(mBrigadeRows, can_edit && uses_grid);
    setEnabledIfChanged(mBrigadeFileSpacing, can_edit && uses_grid);
    setEnabledIfChanged(mBrigadeRankSpacing, can_edit && uses_grid);
    setEnabledIfChanged(mFormationGuideSize, can_edit && guide_visible);
    setEnabledIfChanged(mFormationParam, can_edit && has_parameter);
    setVisibleIfChanged(mFormationParam, has_parameter);
    setVisibleIfChanged(mFormationParamReset, has_parameter);
    setEnabledIfChanged(mFormationParamReset, can_edit && has_parameter);
    setEnabledIfChanged(mCrowdCopiesAs, false);
    setEnabledIfChanged(mArrayLineBtn,
        !draft.mActive && can_edit && have_prototype);
    setEnabledIfChanged(mArrayRingBtn,
        !draft.mActive && can_edit && have_prototype);
    setEnabledIfChanged(mArrayBuildBtn,
        !draft.mActive && can_edit && have_prototype);
    setEnabledIfChanged(mPickFacingTarget,
        !draft.mActive && have_prototype);
    setEnabledIfChanged(mCrowdConfirmBtn,
        owner && draft.mValidation.mCanCommit && source_unchanged &&
        draft.mAnchorValid);
    setEnabledIfChanged(mCrowdUnpinBtn, owner && draft.mPinned);
    setEnabledIfChanged(mCrowdCancelBtn, owner);

    // Status vocabulary: Ready / Ready with warnings / Blocked. Only genuine
    // blockers (missing prototype / invalid spec via mCanCommit, a legality
    // block via !mAnchorValid, or identity/backing/group change via
    // !source_unchanged) count as Blocked; a best-effort terrain fallback, a
    // missing facing target, an anchor clamp/water-snap, or advisory
    // revision drift are warnings, not failures. A legality failure used to
    // collapse into just a nonempty mAnchorWarning -- indistinguishable from
    // a non-blocking clamp/snap warning -- so Confirm stayed wrongly
    // enabled; mAnchorValid is the resolver's own verdict and settles it.
    std::string status;
    if (blocked)
    {
        status = "Blocked: another Ghost Studio panel owns placement.\n" +
                 draft.mValidation.mMessage;
    }
    else if (owner)
    {
        const bool is_blocked =
            !draft.mValidation.mCanCommit || !source_unchanged ||
            !draft.mAnchorValid;
        const bool has_warning =
            !is_blocked &&
            (draft.mValidation.mPartial || !draft.mAnchorWarning.empty() ||
             !source_reason.empty());
        const char* ready_word = is_blocked
            ? "Blocked" : (has_warning ? "Ready with warnings" : "Ready");
        const std::string basis_label =
            ALGhostPlacementResolver::basisLabel(draft.mAnchorBasis);
        status = llformat(
            "%s (%s) \xC2\xB7 %d/%d slots \xC2\xB7 span %.2fm \xC2\xB7 gap %.2fm%s",
            ready_word, basis_label.c_str(),
            (S32)draft.mValidation.mPlacedCount,
            (S32)draft.mValidation.mRequestedCount,
            draft.mValidation.mPlacedRadius,
            draft.mValidation.mActualMinEdgeGap,
            draft.mPinned ? " \xC2\xB7 pinned" : "");
        if (!source_unchanged)
        {
            status = "Blocked: cannot confirm this draft.\n" + source_reason;
        }
        else
        {
            std::string commit_message = mLastCrowdMessage;
            ALToolCrowdPlace* tool = ALToolCrowdPlace::getInstance();
            if (tool->isActive() &&
                tool->armedOwner() == mPlacementOwner &&
                tool->armedSession() == draft.mSessionId &&
                !tool->lastMessage().empty())
            {
                commit_message = tool->lastMessage();
            }
            if (!commit_message.empty())
            {
                status += "\n" + commit_message;
            }
            else if (!draft.mAnchorWarning.empty())
            {
                status += "\n" + draft.mAnchorWarning;
            }
            else if (!source_reason.empty())
            {
                status += "\n" + source_reason;
            }
            else
            {
                status += "\n" + draft.mValidation.mMessage;
            }
        }
    }
    else
    {
        status = mLastCrowdMessage.empty()
            ? "Ready. Select a clone, choose a formation, then Preview in world."
            : mLastCrowdMessage;
    }
    setTextIfChanged(mCrowdPlacementStatus, status);
}

void ALPanelGhostStudio::onClickCrowdConfirm()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::CrowdPlacementDraft draft =
        studio.getCrowdPlacementDraft();
    if (!draft.mActive || draft.mOwnerId != mPlacementOwner)
    {
        return;
    }

    ALToolCrowdPlace* tool = ALToolCrowdPlace::getInstance();
    if (tool->isActive() &&
        tool->armedOwner() == mPlacementOwner &&
        tool->armedSession() == draft.mSessionId)
    {
        tool->commitNow();
        mLastCrowdMessage = tool->lastMessage();
    }
    else
    {
        const ALGhostStudio::PlacementCommitResult result =
            studio.commitCrowdPlacement(
                mPlacementOwner, draft.mSessionId);
        mLastCrowdMessage = result.mMessage;
    }
    mListGate.reset(); // [GhostPanelPerf] force a rebuild on the next refreshList()
    refreshCrowdPlacement();
}

void ALPanelGhostStudio::onClickCrowdUnpin()
{
    const ALGhostStudio::CrowdPlacementDraft& draft =
        ALGhostStudio::instance().getCrowdPlacementDraft();
    if (!draft.mActive || draft.mOwnerId != mPlacementOwner)
    {
        return;
    }
    ALToolCrowdPlace* tool = ALToolCrowdPlace::getInstance();
    if (tool->isActive() &&
        tool->armedOwner() == mPlacementOwner &&
        tool->armedSession() == draft.mSessionId)
    {
        tool->unpin();
    }
    else
    {
        ALGhostStudio::instance().setCrowdPlacementPinned(
            mPlacementOwner, false);
    }
}

void ALPanelGhostStudio::onClickCrowdCancel()
{
    const ALGhostStudio::CrowdPlacementDraft& draft =
        ALGhostStudio::instance().getCrowdPlacementDraft();
    if (!draft.mActive || draft.mOwnerId != mPlacementOwner)
    {
        return;
    }
    ALToolCrowdPlace* tool = ALToolCrowdPlace::getInstance();
    if (tool->isActive() &&
        tool->armedOwner() == mPlacementOwner &&
        tool->armedSession() == draft.mSessionId)
    {
        tool->stopPlacement(true, true);
    }
    else
    {
        ALGhostStudio::instance().cancelCrowdPlacement(mPlacementOwner);
    }
    mLastCrowdMessage = "Placement cancelled.";
    refreshCrowdPlacement();
}

void ALPanelGhostStudio::releaseOwnedCrowdPlacement()
{
    const ALGhostStudio::CrowdPlacementDraft& draft =
        ALGhostStudio::instance().getCrowdPlacementDraft();
    if (!draft.mActive || draft.mOwnerId != mPlacementOwner)
    {
        return;
    }
    ALToolCrowdPlace* tool = ALToolCrowdPlace::getInstance();
    if (tool->isActive() &&
        tool->armedOwner() == mPlacementOwner &&
        tool->armedSession() == draft.mSessionId)
    {
        tool->stopPlacement(true, true);
    }
    else
    {
        ALGhostStudio::instance().cancelCrowdPlacement(mPlacementOwner);
    }
}

void ALPanelGhostStudio::onFormationCommit()
{
    mDetailDirty = true; mCrowdDirty = true; // [GhostPanelPerf] enables depend on this widget's value
    if (!mApplyingCrowdDraft)
    {
        // The combo already contains the new formation. Save the generic
        // parameter under the previously configured formation before reusing
        // that widget for a different meaning.
        captureFormationParameter();
    }
    const ALGhostStudio::EFormation formation =
        (ALGhostStudio::EFormation)mFormationCombo->getValue().asInteger();
    configureFormationParameter(formation);
    if (!mApplyingCrowdDraft)
    {
        ALGhostStudio::EFormationFacing natural =
            ALGhostStudio::FACING_AUTHOR;
        switch (formation)
        {
        case ALGhostStudio::FORMATION_BRIGADE:
            natural = ALGhostStudio::FACING_SOURCE; break;
        case ALGhostStudio::FORMATION_RING:
        case ALGhostStudio::FORMATION_ARC:
        case ALGhostStudio::FORMATION_CONCENTRIC_RINGS:
        case ALGhostStudio::FORMATION_HORSESHOE:
        case ALGhostStudio::FORMATION_RING_BRIGADE:
            natural = ALGhostStudio::FACING_CENTROID_IN; break;
        case ALGhostStudio::FORMATION_SPLIT_ROW:
        case ALGhostStudio::FORMATION_SOUL_TRAIN:
            natural = ALGhostStudio::FACING_AISLE; break;
        case ALGhostStudio::FORMATION_SOUL_TRAIN_MILITARY:
            natural = ALGhostStudio::FACING_FACE_ACROSS; break;
        case ALGhostStudio::FORMATION_CLUSTERS:
            natural = ALGhostStudio::FACING_CLUSTER_IN; break;
        default:
            break;
        }
        mFormationFacing->setValue((S32)natural);
    }
    updateOwnedCrowdPlacement();
}

void ALPanelGhostStudio::onClickStartStrip()
{
    const ALGhostStudio::EFormation formation =
        (ALGhostStudio::EFormation)mFormationCombo->getValue().asInteger();
    if (!formationSupportsFreezeStrip(formation))
    {
        mStripStatus->setText(std::string(
            "Freeze Strip unavailable for this formation; "
            "choose a classic layout above."));
        return;
    }
    mActiveStrip = ALGhostStudio::instance().startFreezeStrip(
        selectedInstance(), mStripCount->getValue().asInteger(),
        (F32)mStripInterval->getValue().asReal(),
        (F32)mArraySpacing->getValue().asReal(),
        formation,
        formation == ALGhostStudio::FORMATION_ARC
            ? mFormationArcSweep : 0.f);
    mStripCancelBtn->setEnabled(mActiveStrip != 0);
}

void ALPanelGhostStudio::onClickCancelStrip()
{
    if (mActiveStrip)
    {
        ALGhostStudio::instance().cancelFreezeStrip(mActiveStrip);
        mActiveStrip = 0;
    }
    mStripCancelBtn->setEnabled(false);
}

void ALPanelGhostStudio::onMotionCommit()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const std::vector<LLUUID> selected = selectedInstances();
    for (const LLUUID& id : selected)
    {
        if (studio.groupForMember(id))
        {
            return;
        }
    }
    studio.setFormationMotion(
        selected,
        (ALGhostStudio::EMotion)mMotionCombo->getValue().asInteger(),
        (F32)mMotionSpeed->getValue().asReal(),
        (F32)mMotionAmplitude->getValue().asReal());
}

void ALPanelGhostStudio::onClickPickFacingTarget()
{
    const LLUUID id = selectedInstance();
    const ALGhostStudio::Instance* prototype =
        ALGhostStudio::instance().getInstance(id);
    if (!prototype || prototype->mGroupId.notNull())
    {
        mLastCrowdMessage =
            "Select an ungrouped clone before picking a facing point.";
        return;
    }

    exitPlaceMode();
    exitEditMode();
    mCrowdFacingPrototype = id;
    const LLHandle<ALPanelGhostStudio> panel_handle =
        getDerivedHandle<ALPanelGhostStudio>();
    ALToolGhostPlace* tool = ALToolGhostPlace::getInstance();
    if (!tool->armForPointPick(
            mPlacementOwner,
            [panel_handle, id](
                bool accepted, const LLVector3d& point_global)
            {
                ALPanelGhostStudio* panel = panel_handle.get();
                if (!panel)
                {
                    return;
                }
                panel->onCrowdFacingPointPicked(
                    id, accepted && panel->isInVisibleChain(),
                    point_global);
            }))
    {
        mCrowdFacingPrototype.setNull();
        mLastCrowdMessage =
            "Another Ghost Studio panel is already picking a world point.";
        return;
    }
    LLToolMgr::getInstance()->setTransientTool(tool);
    mLastCrowdMessage =
        "Click a world point for the crowd to face. Esc cancels.";
}

void ALPanelGhostStudio::onCrowdFacingPointPicked(
    const LLUUID& prototype_id, bool accepted,
    const LLVector3d& point_global)
{
    if (mCrowdFacingPrototype != prototype_id)
    {
        return;
    }
    if (!accepted)
    {
        mLastCrowdMessage = mCrowdFacingPointValid
            ? "Facing-point pick cancelled; the previous point is unchanged."
            : "Facing-point pick cancelled.";
        return;
    }
    if (selectedInstance() != prototype_id ||
        !ALGhostStudio::instance().getInstance(prototype_id) ||
        !point_global.isFinite())
    {
        mCrowdFacingPointValid = false;
        mCrowdFacingPrototype.setNull();
        mLastCrowdMessage =
            "Facing point ignored because the prototype changed.";
        return;
    }

    mCrowdFacingPointGlobal = point_global;
    mCrowdFacingPointValid = true;
    mFormationFacing->setValue(
        (S32)ALGhostStudio::FACING_WORLD_POINT);
    mLastCrowdMessage = llformat(
        "Facing point set at %.2f, %.2f, %.2f.",
        point_global.mdV[VX], point_global.mdV[VY],
        point_global.mdV[VZ]);
    updateOwnedCrowdPlacement();
}

void ALPanelGhostStudio::syncCrowdFacingPointPrototype()
{
    if (mCrowdFacingPrototype.isNull())
    {
        return;
    }
    const LLUUID selected = selection().mInstance; // [GhostPanelPerf] cached, no per-frame LLSD/UUID work
    const ALGhostStudio::Instance* prototype =
        ALGhostStudio::instance().getInstance(selected);
    if (selected == mCrowdFacingPrototype && prototype &&
        prototype->mGroupId.isNull())
    {
        return;
    }

    if (ALToolGhostPlace::getInstance()->isPointPickArmed() &&
        ALToolGhostPlace::getInstance()->armedOwner() == mPlacementOwner)
    {
        exitPlaceMode();
    }
    mCrowdFacingPrototype.setNull();
    mCrowdFacingPointGlobal.setZero();
    mCrowdFacingPointValid = false;
}

void ALPanelGhostStudio::onClickGroup()
{
    ALGhostStudio::instance().lockGroup(
        selectedInstances(), ALGhostStudio::LOCK_RIGID_UNIT);
}

void ALPanelGhostStudio::onClickUngroup()
{
    ALGhostStudio::instance().ungroup(selectedInstance());
}

void ALPanelGhostStudio::onLockModeCommit()
{
    const auto mode =
        (ALGhostStudio::ELockMode)mLockMode->getValue().asInteger();
    const LLUUID id = selectedInstance();
    if (id.notNull())
    {
        ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(id);
        if (inst && inst->mGroupId.notNull())
            ALGhostStudio::instance().setLockMode(id, mode);
        else if (mode != ALGhostStudio::LOCK_OFF &&
                 selectedInstances().size() > 1)
            ALGhostStudio::instance().lockGroup(selectedInstances(), mode);
        mListGate.reset(); // [GhostPanelPerf] force a rebuild on the next refreshList()
    }
}

void ALPanelGhostStudio::onGroupEditMembersCommit()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID selected = selectedInstance();
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(selectedListValue());
    if (!group)
    {
        return;
    }
    studio.setGroupEditMembers(
        group->mId, mGroupEditMembersCheck->get());
    studio.setSelected(selected);
    mListGate.reset(); // [GhostPanelPerf] force a rebuild on the next refreshList()
}

void ALPanelGhostStudio::onGroupMemberPinnedCommit()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row_value = selectedListValue();
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(row_value);
    if (!group || !group->mEditMembers || row_value == group->mId)
    {
        return;
    }
    studio.setGroupMemberPinned(
        row_value, mGroupMemberPinnedCheck->get());
    mListGate.reset(); // [GhostPanelPerf] force a rebuild on the next refreshList()
}

void ALPanelGhostStudio::onClickGroupMemberReset()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row_value = selectedListValue();
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(row_value);
    if (!group || !group->mEditMembers || row_value == group->mId)
    {
        return;
    }
    studio.resetGroupMemberOffset(row_value);
    mListGate.reset(); // [GhostPanelPerf] force a rebuild on the next refreshList()
}

void ALPanelGhostStudio::onNameplateModeCommit()
{
    gSavedSettings.setS32(
        "GhostStudioNameplateMode",
        llclamp(mNameplateMode->getValue().asInteger(), 0, 2));
}

void ALPanelGhostStudio::onPoseRateCommit()
{
    ALGhostStudio::instance().setPoseRate(
        selectedInstance(), (F32)mPoseRate->getValue().asReal());
}

void ALPanelGhostStudio::onShowAllToggle()
{
    const bool visible = mShowAllCheck->get();
    ALGhostStudio::instance().setShowAll(visible);
    if (!visible)
    {
        exitEditMode();
    }
}

void ALPanelGhostStudio::onResetNumeric(const std::string& target_name)
{
    // getChild<T> manufactures a fallback widget of T on a miss, so T must be
    // constructible by the factory -- LLF32UICtrl's constructor is protected, so
    // asking for it directly fails to compile. Look the child up as the generic
    // LLUICtrl and narrow it instead; a miss or a non-numeric target then simply
    // does nothing rather than resetting the wrong control.
    LLUICtrl* ctrl = findChild<LLUICtrl>(target_name);
    LLF32UICtrl* target = dynamic_cast<LLF32UICtrl*>(ctrl);
    if (!target || !target->getEnabled())
    {
        return;
    }
    target->setValue(target->getInitialValue());
    target->onCommit();
}
