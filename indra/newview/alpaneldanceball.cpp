/**
 * @file alpaneldanceball.cpp
 * @brief Dance Ball control panel -- see the header.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Hosted by floater_dance_ball.xml, which embeds "panel_dance_ball.xml".
 */

#include "llviewerprecompiledheaders.h"

#include "alpaneldanceball.h"

#include "alghoststudio.h"
#include "animationexplorer.h"      // AnimationExplorer::getSelectedAnimId()
#include "llagent.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llfile.h"
#include "llfloaterreg.h"
#include "llinventory.h"            // LLInventoryItem / LLInventoryCategory
#include "llinventorymodel.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llstring.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "llviewerinventory.h"
#include "llviewermenufile.h"       // LLFilePickerReplyThread
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
#include <vector>

// defined in llviewermenu.cpp (also declared locally by alviewermenu.cpp)
LLVOAvatar* find_avatar_from_object(LLViewerObject* object);

// The injector name must match class="panel_dance_ball" in the host XML
// (floater_dance_ball.xml embeds "panel_dance_ball.xml"). Without the embed
// nothing instantiates this panel.
static LLPanelInjector<ALPanelDanceBall> t_panel_dance_ball("panel_dance_ball");

namespace
{
using Kind = ALDanceBall::DancerKind;
using State = ALDanceBall::DancerState;

constexpr std::size_t MAX_DROP_ANIMATIONS = 200;
constexpr std::streamoff MAX_SESSION_FILE_BYTES = 8 * 1024 * 1024;

// A gate used for structural and control refreshes. The self-heal of
// ALRevisionGate would otherwise rebuild a list once a second even though no
// watched revision moved; lists are rebuilt only for REAL revision changes
// (or the UIRefreshForceAlways debug switch).
bool gate_due(ALRevisionGate& gate, std::initializer_list<U64> revisions)
{
    if (!gate.isStale(revisions))
    {
        return false;
    }
    const bool fallback_only = gate.lastWasFallback();
    gate.accept(revisions);
    return !fallback_only || alUiRefreshForceAlways();
}

void set_cell(LLScrollListItem* item, S32 column, std::string& cached,
              const std::string& value)
{
    if (cached == value)
    {
        return;
    }
    if (LLScrollListCell* cell = item->getColumn(column))
    {
        cell->setValue(LLSD(value));
        cached = value;
    }
}

std::string trimmed(std::string text)
{
    LLStringUtil::trim(text);
    return text;
}
} // anonymous namespace

ALPanelDanceBall::ALPanelDanceBall() = default;
ALPanelDanceBall::~ALPanelDanceBall() = default;

// ---------------------------------------------------------------------------
// build
// ---------------------------------------------------------------------------

bool ALPanelDanceBall::postBuild()
{
    mOnBtn = getChild<LLButton>("db_on");
    mPrevBtn = getChild<LLButton>("db_prev");
    mNextBtn = getChild<LLButton>("db_next");
    mFreezeBtn = getChild<LLButton>("db_freeze");
    mShuffleBtn = getChild<LLButton>("db_shuffle");
    mStopAllBtn = getChild<LLButton>("db_stop_all");
    mHeaderDesync = getChild<LLSliderCtrl>("db_header_desync");
    mStatusText = getChild<LLTextBox>("db_status");
    mMessageText = getChild<LLTextBox>("db_message");
    mTabs = getChild<LLTabContainer>("db_tabs");

    mDancerList = getChild<LLScrollListCtrl>("db_dancer_list");
    mAddSelectedBtn = getChild<LLButton>("db_add_selected");
    mAddCastBtn = getChild<LLButton>("db_add_cast");
    mAddGhostsBtn = getChild<LLButton>("db_add_ghosts");
    mRemoveBtn = getChild<LLButton>("db_remove");
    mExcludeBtn = getChild<LLButton>("db_exclude");
    mAddNearbyBtn = getChild<LLButton>("db_add_nearby");
    mClearExclusionsBtn = getChild<LLButton>("db_clear_exclusions");
    mRadius = getChild<LLSpinCtrl>("db_radius");
    mFocus = getChild<LLComboBox>("db_focus");
    mAutoJoin = getChild<LLCheckBoxCtrl>("db_autojoin");
    mIncludeSeated = getChild<LLCheckBoxCtrl>("db_include_seated");
    mIncludeMe = getChild<LLCheckBoxCtrl>("db_include_me");

    mPlaylistList = getChild<LLScrollListCtrl>("db_playlist_list");
    mFromExplorerBtn = getChild<LLButton>("db_from_explorer");
    mPasteBtn = getChild<LLButton>("db_paste_uuid");
    mPlRemoveBtn = getChild<LLButton>("db_pl_remove");
    mPlClearBtn = getChild<LLButton>("db_pl_clear");
    mPlUpBtn = getChild<LLButton>("db_pl_up");
    mPlDownBtn = getChild<LLButton>("db_pl_down");
    mPlRenameBtn = getChild<LLButton>("db_pl_rename");
    mRenameEdit = getChild<LLLineEditor>("db_rename_edit");
    mPlaySelectedBtn = getChild<LLButton>("db_play_selected");
    mPlayAllBtn = getChild<LLButton>("db_play_all");
    mMode = getChild<LLComboBox>("db_mode");
    mAutoAdvance = getChild<LLCheckBoxCtrl>("db_auto_advance");
    mAdvInterval = getChild<LLSpinCtrl>("db_adv_interval");
    mAdvJitter = getChild<LLSpinCtrl>("db_adv_jitter");

    mTDesync = getChild<LLSliderCtrl>("db_t_desync");
    mReshuffleBtn = getChild<LLButton>("db_reshuffle");
    mTSpeed = getChild<LLSliderCtrl>("db_t_speed");
    mTSpeedJitter = getChild<LLSliderCtrl>("db_t_speed_jitter");
    mTCrossfade = getChild<LLSliderCtrl>("db_t_crossfade");
    mTPriority = getChild<LLComboBox>("db_t_priority");
    mTDanceDrivesFace = getChild<LLCheckBoxCtrl>("db_t_dance_drives_face");

    mWaveEnable = getChild<LLCheckBoxCtrl>("db_wave_enable");
    mWaveFocus = getChild<LLComboBox>("db_wave_focus");
    mWaveSpeed = getChild<LLSliderCtrl>("db_wave_speed");
    mWaveDir = getChild<LLComboBox>("db_wave_dir");
    mWaveApply = getChild<LLComboBox>("db_wave_apply");
    mRampTarget = getChild<LLSpinCtrl>("db_ramp_target");
    mRampSeconds = getChild<LLSpinCtrl>("db_ramp_seconds");
    mRampGoBtn = getChild<LLButton>("db_ramp_go");
    mExportBtn = getChild<LLButton>("db_export");
    mImportBtn = getChild<LLButton>("db_import");

    // XML strings, cached once so no getString() ever runs per frame.
    mStrKind[0] = getString("kind_self");
    mStrKind[1] = getString("kind_resident");
    mStrKind[2] = getString("kind_animesh");
    mStrKind[3] = getString("kind_ghost");
    mStrState[0] = getString("state_idle");
    mStrState[1] = getString("state_playing");
    mStrState[2] = getString("state_pending");
    mStrState[3] = getString("state_failed");
    mStrState[4] = getString("state_server");
    mStrState[5] = getString("state_seated");
    mStrState[6] = getString("state_muted");
    mStrState[7] = getString("state_moving");
    mStrState[8] = getString("state_absent");
    mStrState[9] = getString("state_in_use");
    mStrState[10] = getString("state_taken_over");
    mStrOnYes = getString("on_yes");
    mStrOnNo = getString("on_no");
    mStrNoValue = getString("no_value");
    mStrNameLoading = getString("name_loading");
    mStrNameAnimesh = getString("name_animesh");
    mStrNameGhost = getString("name_ghost");
    mStrDistFormat = getString("distance_format");
    mStrLengthFormat = getString("length_format");
    mStrLoopYes = getString("loop_yes");
    mStrLoopNo = getString("loop_no");
    mStrItem[0] = getString("item_pending");
    mStrItem[1] = getString("item_ready");
    mStrItem[2] = getString("item_unavailable");
    mStrStatusOff = getString("status_off");
    mStrStatusOn = getString("status_on");
    mStrStatusFrozen = getString("status_frozen");
    mStrStatusPlaying = getString("status_playing");
    mStrMode[0] = getString("mode_same");
    mStrMode[1] = getString("mode_shuffle");
    mStrMode[2] = getString("mode_sequence");
    mStrSyncSynced = getString("sync_synced");
    mStrSyncDesync = getString("sync_desync");

    // header
    mOnBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onToggleOn(); });
    mPrevBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPrevious(); });
    mNextBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onNext(); });
    mFreezeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onFreeze(); });
    mShuffleBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShuffle(); });
    mStopAllBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStopAll(); });
    mHeaderDesync->setCommitCallback([this](LLUICtrl*, const LLSD&) { onHeaderDesync(); });

    // dancers
    mAddSelectedBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAddSelected(); });
    mAddCastBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAddCast(); });
    mAddGhostsBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAddGhosts(); });
    mAddNearbyBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAddNearby(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onRemove(); });
    mExcludeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onExclude(); });
    mClearExclusionsBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClearExclusions(); });
    mRadius->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoJoinChanged(); });
    mFocus->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoJoinChanged(); });
    mAutoJoin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoJoinChanged(); });
    mIncludeSeated->setCommitCallback([this](LLUICtrl*, const LLSD&) { onIncludeSeated(); });
    mIncludeMe->setCommitCallback([this](LLUICtrl*, const LLSD&) { onIncludeMe(); });

    // playlist
    mFromExplorerBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onFromExplorer(); });
    mPasteBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPasteUUID(); });
    mPlRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaylistRemove(); });
    mPlClearBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaylistClear(); });
    mPlUpBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaylistUp(); });
    mPlDownBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaylistDown(); });
    mPlRenameBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaylistRename(); });
    mRenameEdit->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaylistRename(); });
    mPlaylistList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaylistSelection(); });
    mPlaySelectedBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlaySelected(); });
    mPlayAllBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlayAll(); });
    mMode->setCommitCallback([this](LLUICtrl*, const LLSD&) { onModeChanged(); });
    mAutoAdvance->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAdvanceChanged(); });
    mAdvInterval->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAdvanceChanged(); });
    mAdvJitter->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAutoAdvanceChanged(); });

    // timing
    mTDesync->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTimingChanged(false); });
    mTSpeed->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTimingChanged(true); });
    mTSpeedJitter->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTimingChanged(false); });
    mTCrossfade->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTimingChanged(false); });
    mTPriority->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTimingChanged(false); });
    mTDanceDrivesFace->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTimingChanged(false); });
    mReshuffleBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onReshuffle(); });

    // cinema
    mWaveEnable->setCommitCallback([this](LLUICtrl*, const LLSD&) { onWaveChanged(); });
    mWaveFocus->setCommitCallback([this](LLUICtrl*, const LLSD&) { onWaveChanged(); });
    mWaveSpeed->setCommitCallback([this](LLUICtrl*, const LLSD&) { onWaveChanged(); });
    mWaveDir->setCommitCallback([this](LLUICtrl*, const LLSD&) { onWaveChanged(); });
    mWaveApply->setCommitCallback([this](LLUICtrl*, const LLSD&) { onWaveChanged(); });
    mRampGoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onRampGo(); });
    mExportBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onExport(); });
    mImportBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onImport(); });

    // Wrap + fit every tab page, re-fit on tab change, and enable the wheel on
    // every slider under this panel (ALScrollFit; CLAUDE.md UI mandate).
    mScrollFit.installForTabs(mTabs, this);

    requestRefresh();
    return true;
}

void ALPanelDanceBall::onVisibilityChange(bool new_visibility)
{
    LLPanel::onVisibilityChange(new_visibility);
    if (new_visibility)
    {
        // First frame after a (re)show is a full refresh, not whatever the
        // gates remembered; documents are re-fitted at open time.
        requestRefresh();
        if (mTabs)
        {
            mScrollFit.fit(this, mTabs);
        }
    }
}

// ---------------------------------------------------------------------------
// draw: revision checks and diff-guarded writes only
// ---------------------------------------------------------------------------

void ALPanelDanceBall::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    // At most 2 Hz. User actions call requestRefresh() so their result shows
    // on the very next frame.
    if (mVolatileClock.due())
    {
        const ALDanceBall& ball = ALDanceBall::instance();
        const U64 roster_rev = ball.rosterRevision();
        const U64 playlist_rev = ball.playlistRevision();
        const U64 status_rev = ball.statusRevision();
        // The same-for-all "Now" label moves on auto-advance without a status
        // bump, so the current item (+1 so "nothing playing" is 0) gates it too.
        const U64 same_item = static_cast<U64>(ball.sameForAllCurrentItem() + 1);

        if (isWidgetOnScreen(mStatusText) &&
            gate_due(mHeaderGate, { roster_rev, status_rev, playlist_rev, same_item }))
        {
            refreshHeader(ball);
        }

        if (isWidgetOnScreen(mDancerList))
        {
            if (gate_due(mDancerStructGate, { roster_rev }))
            {
                rebuildDancerList(ball);
            }
            // distance and now-playing move without any revision: 2 Hz tick
            updateDancerCells(ball);
            if (gate_due(mDancerControlGate, { status_rev }))
            {
                refreshDancerControls(ball);
            }
        }

        if (isWidgetOnScreen(mPlaylistList))
        {
            if (gate_due(mPlaylistStructGate, { playlist_rev }))
            {
                rebuildPlaylistList(ball);
            }
            if (gate_due(mPlaylistCellGate, { status_rev, playlist_rev }))
            {
                updatePlaylistCells(ball);
            }
            if (gate_due(mPlaylistControlGate, { status_rev }))
            {
                refreshPlaylistControls(ball);
            }
        }

        if (isWidgetOnScreen(mTDesync) &&
            gate_due(mTimingGate, { status_rev }))
        {
            refreshTimingControls(ball);
        }
        // After the timing refresh, so a live ramp value wins in the same tick.
        if (isWidgetOnScreen(mTSpeed))
        {
            refreshLiveSpeed(ball);
        }

        if (isWidgetOnScreen(mWaveEnable) &&
            gate_due(mCinemaGate, { status_rev }))
        {
            refreshCinemaControls(ball);
        }
    }
    LLPanel::draw();
}

void ALPanelDanceBall::requestRefresh()
{
    mHeaderGate.reset();
    mDancerStructGate.reset();
    mPlaylistStructGate.reset();
    mPlaylistCellGate.reset();
    mDancerControlGate.reset();
    mPlaylistControlGate.reset();
    mTimingGate.reset();
    mCinemaGate.reset();
    mVolatileClock.forceDue();
}

// ---------------------------------------------------------------------------
// text helpers
// ---------------------------------------------------------------------------

std::string ALPanelDanceBall::formatString(
    const std::string& tmpl, const LLStringUtil::format_map_t& args) const
{
    std::string text = tmpl;
    LLStringUtil::format(text, args);
    return text;
}

void ALPanelDanceBall::setMessage(const std::string& text)
{
    // event time only (button handlers, picker callbacks)
    if (mMessageText)
    {
        mMessageText->setText(text);
    }
}

void ALPanelDanceBall::setMessageFormat(const std::string& tmpl,
                                        const char* key,
                                        const std::string& value)
{
    LLStringUtil::format_map_t args;
    args[key] = value;
    setMessage(formatString(tmpl, args));
}

std::string ALPanelDanceBall::kindText(ALDanceBall::DancerKind kind) const
{
    const std::size_t index = static_cast<std::size_t>(kind);
    return index < 4 ? mStrKind[index] : mStrNoValue;
}

std::string ALPanelDanceBall::stateText(ALDanceBall::DancerState state) const
{
    const std::size_t index = static_cast<std::size_t>(state);
    return index < 11 ? mStrState[index] : mStrNoValue;
}

std::string ALPanelDanceBall::itemStatusText(
    ALDanceBall::AssetStatus status) const
{
    const std::size_t index = static_cast<std::size_t>(status);
    return index < 3 ? mStrItem[index] : mStrNoValue;
}

std::string ALPanelDanceBall::dancerName(const ALDanceBall::Dancer& dancer,
                                         LLVOAvatar*,
                                         const std::string& short_id) const
{
    switch (dancer.mKind)
    {
    case Kind::SELF:
    case Kind::RESIDENT:
    {
        LLAvatarName avatar_name;
        if (LLAvatarNameCache::get(dancer.mId, &avatar_name))
        {
            return avatar_name.getDisplayName();
        }
        return mStrNameLoading;
    }
    case Kind::ANIMESH:
    {
        if (const LLDirectorCast::CastMember* member =
                LLDirectorCast::instance().getMember(dancer.mId))
        {
            if (!member->mLastName.empty())
            {
                return member->mLastName;
            }
        }
        LLStringUtil::format_map_t args;
        args["[ID]"] = short_id;
        return formatString(mStrNameAnimesh, args);
    }
    case Kind::GHOST:
    {
        if (const ALGhostStudio::Instance* instance =
                ALGhostStudio::instance().getInstance(dancer.mId))
        {
            if (!instance->mName.empty())
            {
                return instance->mName;
            }
        }
        LLStringUtil::format_map_t args;
        args["[ID]"] = short_id;
        return formatString(mStrNameGhost, args);
    }
    }
    return mStrNoValue;
}

std::string ALPanelDanceBall::dancerDistance(
    const ALDanceBall::Dancer& dancer, LLVOAvatar* avatar) const
{
    if (!avatar || !dancer.mPresent || avatar->isDead())
    {
        return mStrNoValue;
    }
    const F64 meters = dist_vec(avatar->getPositionGlobal(),
                                gAgent.getPositionGlobal());
    LLStringUtil::format_map_t args;
    args["[METERS]"] = llformat("%.1f", meters);
    return formatString(mStrDistFormat, args);
}

// ---------------------------------------------------------------------------
// resolution helpers
// ---------------------------------------------------------------------------

//static
LLVOAvatar* ALPanelDanceBall::avatarFromObject(LLViewerObject* object)
{
    if (!object)
    {
        return nullptr;
    }
    // A worn animesh targets its OWN control avatar (same rule as
    // "Light Response..."); find_avatar_from_object() walks an attachment up
    // to the wearer.
    if (!object->isAvatar())
    {
        LLControlAvatar* control = object->getControlAvatar();
        if (control && !control->isDead())
        {
            return control;
        }
    }
    return find_avatar_from_object(object);
}

//static
ALDanceBall::DancerKind ALPanelDanceBall::kindForAvatar(LLVOAvatar* avatar)
{
    // mirrors ALDanceBall::classify (private there)
    if (!avatar)
    {
        return Kind::RESIDENT;
    }
    if (avatar->isSelf() || avatar->getID() == gAgentID)
    {
        return Kind::SELF;
    }
    if (avatar->isGhostAvatar() ||
        ALGhostStudio::instance().findInstanceByRuntime(avatar->getID()))
    {
        return Kind::GHOST;
    }
    return avatar->isControlAvatar() ? Kind::ANIMESH : Kind::RESIDENT;
}

//static
LLUUID ALPanelDanceBall::rosterIdFor(LLVOAvatar* avatar, Kind kind)
{
    if (!avatar)
    {
        return LLUUID::null;
    }
    switch (kind)
    {
    case Kind::SELF:
        return gAgentID;
    case Kind::ANIMESH:
        return LLDirectorCast::canonicalActorId(avatar->getID());
    case Kind::GHOST:
        if (const ALGhostStudio::Instance* instance =
                ALGhostStudio::instance().findInstanceByRuntime(avatar->getID()))
        {
            return instance->mId;
        }
        return avatar->getID();
    case Kind::RESIDENT:
    default:
        return avatar->getID();
    }
}

LLVOAvatar* ALPanelDanceBall::resolveDancer(
    const ALDanceBall::Dancer& dancer) const
{
    switch (dancer.mKind)
    {
    case Kind::SELF:
        return isAgentAvatarValid() ? gAgentAvatarp.get() : nullptr;
    case Kind::RESIDENT:
        return dynamic_cast<LLVOAvatar*>(gObjectList.findObject(dancer.mId));
    case Kind::ANIMESH:
        return LLDirectorCast::instance().resolve(dancer.mId);
    case Kind::GHOST:
        if (const ALGhostStudio::Instance* instance =
                ALGhostStudio::instance().getInstance(dancer.mId))
        {
            return dynamic_cast<LLVOAvatar*>(
                gObjectList.findObject(instance->mEntityId));
        }
        return nullptr;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// shared entry points
// ---------------------------------------------------------------------------

//static
bool ALPanelDanceBall::isAvatarDancer(LLVOAvatar* avatar)
{
    if (!avatar)
    {
        return false;
    }
    const LLUUID id = rosterIdFor(avatar, kindForAvatar(avatar));
    if (id.isNull())
    {
        return false;
    }
    const std::vector<ALDanceBall::Dancer>& roster =
        ALDanceBall::instance().roster();
    return std::any_of(roster.begin(), roster.end(),
        [&](const ALDanceBall::Dancer& dancer) { return dancer.mId == id; });
}

//static
void ALPanelDanceBall::toggleAvatar(LLVOAvatar* avatar)
{
    if (!avatar)
    {
        return;
    }
    ALDanceBall& ball = ALDanceBall::instance();
    const Kind kind = kindForAvatar(avatar);
    const LLUUID id = rosterIdFor(avatar, kind);
    if (id.isNull())
    {
        return;
    }
    if (isAvatarDancer(avatar))
    {
        ball.removeDancer(id);
        return;
    }
    // An explicit per-avatar action overrides an earlier Exclude.
    ball.include(id);
    if (addAvatarToBall(avatar))
    {
        LLFloaterReg::showInstance("dance_ball");
    }
}

//static
bool ALPanelDanceBall::addAnimationAsset(const LLUUID& asset_id,
                                         const std::string& label)
{
    if (asset_id.isNull())
    {
        return false;
    }
    return ALDanceBall::instance().addAnimation(asset_id, label);
}

// ---------------------------------------------------------------------------
// refresh
// ---------------------------------------------------------------------------

void ALPanelDanceBall::refreshHeader(const ALDanceBall& ball)
{
    const ALDanceBall::Timing& timing = ball.timing();
    setValueIfChanged(mOnBtn, LLSD(ball.enabled()), true);
    setValueIfChanged(mFreezeBtn, LLSD(ball.frozen()), true);
    setValueIfChanged(mShuffleBtn,
        LLSD(timing.mMode ==
             ALDanceBallModel::PlaylistMode::SHUFFLE_EACH_DANCER), true);
    setValueIfChanged(mHeaderDesync, LLSD(timing.mDesync));

    const std::vector<ALDanceBall::Dancer>& roster = ball.roster();
    S32 absent = 0;
    for (const ALDanceBall::Dancer& dancer : roster)
    {
        if (dancer.mState == State::ABSENT)
        {
            ++absent;
        }
    }

    LLStringUtil::format_map_t args;
    args["[DANCERS]"] = llformat("%d", static_cast<S32>(roster.size()));
    std::string text;
    if (!ball.enabled())
    {
        text = formatString(mStrStatusOff, args);
    }
    else
    {
        args["[ABSENT]"] = llformat("%d", absent);
        const std::size_t mode_index =
            std::min<std::size_t>(static_cast<std::size_t>(timing.mMode), 2);
        args["[MODE]"] = mStrMode[mode_index];
        if (timing.mDesync <= 0.0005)
        {
            args["[SYNC]"] = mStrSyncSynced;
        }
        else
        {
            LLStringUtil::format_map_t sync_args;
            sync_args["[PERCENT]"] =
                llformat("%d", static_cast<S32>(timing.mDesync * 100.0 + 0.5));
            args["[SYNC]"] = formatString(mStrSyncDesync, sync_args);
        }
        text = formatString(mStrStatusOn, args);
        if (timing.mMode == ALDanceBallModel::PlaylistMode::SAME_FOR_ALL)
        {
            // empty when nothing is playing in same-for-all mode
            const std::string label = ball.sameForAllCurrentLabel();
            if (!label.empty())
            {
                LLStringUtil::format_map_t label_args;
                label_args["[LABEL]"] = label;
                text += formatString(mStrStatusPlaying, label_args);
            }
        }
        if (ball.frozen())
        {
            text += mStrStatusFrozen;
        }
    }
    setTextIfChanged(mStatusText, text);
}

void ALPanelDanceBall::rebuildDancerList(const ALDanceBall& ball)
{
    mRefreshing = true;
    std::vector<LLUUID> selected;
    for (LLScrollListItem* item : mDancerList->getAllSelected())
    {
        selected.push_back(item->getValue().asUUID());
    }
    const S32 scroll = mDancerList->getScrollPos();

    mDancerList->clearRows();
    mDancerRows.clear();
    for (const ALDanceBall::Dancer& dancer : ball.roster())
    {
        LLVOAvatar* avatar = resolveDancer(dancer);
        DancerRow row;
        row.mShortId = dancer.mShortId;
        row.mOn = dancer.mState == State::PLAYING ? mStrOnYes : mStrOnNo;
        row.mName = dancerName(dancer, avatar, row.mShortId);
        row.mKind = kindText(dancer.mKind);
        row.mNow = nowPlayingText(dancer);
        row.mDist = dancerDistance(dancer, avatar);

        LLSD element;
        element["id"] = dancer.mId;
        element["columns"][0]["column"] = "on";
        element["columns"][0]["value"] = row.mOn;
        element["columns"][1]["column"] = "name";
        element["columns"][1]["value"] = row.mName;
        element["columns"][2]["column"] = "kind";
        element["columns"][2]["value"] = row.mKind;
        element["columns"][3]["column"] = "now";
        element["columns"][3]["value"] = row.mNow;
        element["columns"][4]["column"] = "dist";
        element["columns"][4]["value"] = row.mDist;
        mDancerList->addElement(element);
        mDancerRows[dancer.mId] = row;
    }
    if (!selected.empty())
    {
        mDancerList->selectMultiple(selected);
    }
    mDancerList->setScrollPos(scroll);
    mRefreshing = false;
}

void ALPanelDanceBall::updateDancerCells(const ALDanceBall& ball)
{
    const std::vector<ALDanceBall::Dancer>& roster = ball.roster();
    if (mDancerList->getItemCount() != static_cast<S32>(roster.size()))
    {
        // the list is out of step with the roster: rebuild on the next tick
        mDancerStructGate.reset();
        return;
    }
    for (std::size_t i = 0; i < roster.size(); ++i)
    {
        const ALDanceBall::Dancer& dancer = roster[i];
        LLScrollListItem* item = mDancerList->getItemByIndex(static_cast<S32>(i));
        if (!item || item->getValue().asUUID() != dancer.mId)
        {
            mDancerStructGate.reset();
            return;
        }
        auto found = mDancerRows.find(dancer.mId);
        if (found == mDancerRows.end())
        {
            mDancerStructGate.reset();
            return;
        }
        DancerRow& row = found->second;
        LLVOAvatar* avatar = resolveDancer(dancer);
        set_cell(item, 0, row.mOn,
                 dancer.mState == State::PLAYING ? mStrOnYes : mStrOnNo);
        set_cell(item, 1, row.mName,
                 dancerName(dancer, avatar, row.mShortId));
        set_cell(item, 3, row.mNow, nowPlayingText(dancer));
        set_cell(item, 4, row.mDist, dancerDistance(dancer, avatar));
    }
}

void ALPanelDanceBall::rebuildPlaylistList(const ALDanceBall& ball)
{
    mRefreshing = true;
    S32 keep = mPendingPlaylistSelect >= 0 ? mPendingPlaylistSelect
                                          : selectedPlaylistIndex();
    mPendingPlaylistSelect = -1;
    const S32 scroll = mPlaylistList->getScrollPos();

    mPlaylistList->clearRows();
    mPlaylistRows.clear();
    const std::vector<ALDanceBall::PlaylistEntry>& playlist = ball.playlist();
    for (std::size_t i = 0; i < playlist.size(); ++i)
    {
        const ALDanceBall::PlaylistEntry& entry = playlist[i];
        PlaylistRow row;
        row.mName = entry.mLabel;
        if (entry.mStatus == ALDanceBall::AssetStatus::READY)
        {
            LLStringUtil::format_map_t args;
            args["[SECONDS]"] = llformat("%.1f", entry.mDuration);
            row.mLength = formatString(mStrLengthFormat, args);
            row.mLoop = entry.mLoop ? mStrLoopYes : mStrLoopNo;
        }
        else
        {
            row.mLength = mStrNoValue;
            row.mLoop = mStrNoValue;
        }
        row.mStatus = itemStatusText(entry.mStatus);

        LLSD element;
        element["id"] = static_cast<S32>(i);
        element["columns"][0]["column"] = "name";
        element["columns"][0]["value"] = row.mName;
        element["columns"][1]["column"] = "length";
        element["columns"][1]["value"] = row.mLength;
        element["columns"][2]["column"] = "loop";
        element["columns"][2]["value"] = row.mLoop;
        element["columns"][3]["column"] = "status";
        element["columns"][3]["value"] = row.mStatus;
        mPlaylistList->addElement(element);
        mPlaylistRows.push_back(row);
    }
    if (keep >= 0 && keep < mPlaylistList->getItemCount())
    {
        mPlaylistList->selectNthItem(keep);
    }
    mPlaylistList->setScrollPos(scroll);
    mRefreshing = false;
}

void ALPanelDanceBall::updatePlaylistCells(const ALDanceBall& ball)
{
    const std::vector<ALDanceBall::PlaylistEntry>& playlist = ball.playlist();
    if (mPlaylistList->getItemCount() != static_cast<S32>(playlist.size()) ||
        mPlaylistRows.size() != playlist.size())
    {
        mPlaylistStructGate.reset();
        return;
    }
    for (std::size_t i = 0; i < playlist.size(); ++i)
    {
        const ALDanceBall::PlaylistEntry& entry = playlist[i];
        LLScrollListItem* item = mPlaylistList->getItemByIndex(static_cast<S32>(i));
        if (!item || item->getValue().asInteger() != static_cast<S32>(i))
        {
            mPlaylistStructGate.reset();
            return;
        }
        PlaylistRow& row = mPlaylistRows[i];
        std::string length = mStrNoValue;
        std::string loop = mStrNoValue;
        if (entry.mStatus == ALDanceBall::AssetStatus::READY)
        {
            LLStringUtil::format_map_t args;
            args["[SECONDS]"] = llformat("%.1f", entry.mDuration);
            length = formatString(mStrLengthFormat, args);
            loop = entry.mLoop ? mStrLoopYes : mStrLoopNo;
        }
        set_cell(item, 0, row.mName, entry.mLabel);
        set_cell(item, 1, row.mLength, length);
        set_cell(item, 2, row.mLoop, loop);
        set_cell(item, 3, row.mStatus, itemStatusText(entry.mStatus));
    }
}

void ALPanelDanceBall::refreshDancerControls(const ALDanceBall& ball)
{
    setValueIfChanged(mRadius, LLSD(static_cast<F64>(ball.autoJoinRadius())));
    setValueIfChanged(mFocus,
        LLSD(llformat("%d", static_cast<S32>(ball.autoJoinFocus()))));
    setValueIfChanged(mAutoJoin, LLSD(ball.autoJoin()), true);
    setValueIfChanged(mIncludeSeated, LLSD(ball.includeSeated()), true);
    setValueIfChanged(mIncludeMe, LLSD(ball.includeSelf()), true);
}

void ALPanelDanceBall::refreshPlaylistControls(const ALDanceBall& ball)
{
    const ALDanceBall::Timing& timing = ball.timing();
    setValueIfChanged(mMode,
        LLSD(llformat("%d", static_cast<S32>(timing.mMode))));
    setValueIfChanged(mAutoAdvance, LLSD(timing.mAutoAdvance), true);
    setValueIfChanged(mAdvInterval, LLSD(timing.mAdvanceSeconds));
    setValueIfChanged(mAdvJitter, LLSD(timing.mAdvanceJitterSeconds));
}

void ALPanelDanceBall::refreshTimingControls(const ALDanceBall& ball)
{
    const ALDanceBall::Timing& timing = ball.timing();
    setValueIfChanged(mTDesync, LLSD(timing.mDesync));
    setValueIfChanged(mTSpeed, LLSD(timing.mSpeed));
    setValueIfChanged(mTSpeedJitter, LLSD(timing.mSpeedJitter));
    setValueIfChanged(mTCrossfade, LLSD(timing.mCrossfadeSeconds));
    setValueIfChanged(mTPriority, LLSD(llformat("%d", timing.mPriority)));
    setValueIfChanged(mTDanceDrivesFace, LLSD(timing.mDanceDrivesFace), true);
}

void ALPanelDanceBall::refreshLiveSpeed(const ALDanceBall& ball)
{
    // rampProgress() is 1.0 when no ramp runs, so < 1.0 covers the whole ramp
    // (including its first tick at 0). setValueIfChanged skips the write while
    // the slider or its edit box has focus or a drag holds the mouse.
    if (ball.speedRampProgress() < 1.0)
    {
        mLiveSpeedShown = true;
        setValueIfChanged(mTSpeed, LLSD(ball.liveSpeed()));
    }
    else if (mLiveSpeedShown)
    {
        // the ramp has landed: hand the slider back to the stored speed
        mLiveSpeedShown = false;
        setValueIfChanged(mTSpeed, LLSD(ball.timing().mSpeed));
    }
}

void ALPanelDanceBall::refreshCinemaControls(const ALDanceBall& ball)
{
    const ALDanceBall::Wave& wave = ball.wave();
    setValueIfChanged(mWaveEnable, LLSD(wave.mEnabled), true);
    setValueIfChanged(mWaveFocus,
        LLSD(llformat("%d", static_cast<S32>(wave.mFocus))));
    setValueIfChanged(mWaveSpeed, LLSD(wave.mSpreadMetersPerSecond));
    setValueIfChanged(mWaveDir,
        LLSD(llformat("%d", static_cast<S32>(wave.mDirection))));
    setValueIfChanged(mWaveApply,
        LLSD(llformat("%d", static_cast<S32>(wave.mApply))));
}

S32 ALPanelDanceBall::selectedPlaylistIndex() const
{
    if (LLScrollListItem* item = mPlaylistList->getFirstSelected())
    {
        return item->getValue().asInteger();
    }
    return -1;
}

// ---------------------------------------------------------------------------
// header handlers
// ---------------------------------------------------------------------------

void ALPanelDanceBall::onToggleOn()
{
    ALDanceBall& ball = ALDanceBall::instance();
    ball.setEnabled(mOnBtn->getToggleState());
    requestRefresh();
}

void ALPanelDanceBall::onPrevious()
{
    ALDanceBall::instance().previous();
    requestRefresh();
}

void ALPanelDanceBall::onNext()
{
    ALDanceBall::instance().next();
    requestRefresh();
}

void ALPanelDanceBall::onFreeze()
{
    ALDanceBall::instance().setFrozen(mFreezeBtn->getToggleState());
    requestRefresh();
}

void ALPanelDanceBall::onShuffle()
{
    ALDanceBall& ball = ALDanceBall::instance();
    ALDanceBall::Timing timing = ball.timing();
    timing.mMode = mShuffleBtn->getToggleState()
        ? ALDanceBallModel::PlaylistMode::SHUFFLE_EACH_DANCER
        : ALDanceBallModel::PlaylistMode::SAME_FOR_ALL;
    ball.setTiming(timing);
    requestRefresh();
}

void ALPanelDanceBall::onStopAll()
{
    ALDanceBall::instance().setEnabled(false);
    setMessage(getString("msg_stopped"));
    requestRefresh();
}

void ALPanelDanceBall::onHeaderDesync()
{
    ALDanceBall& ball = ALDanceBall::instance();
    ALDanceBall::Timing timing = ball.timing();
    timing.mDesync = mHeaderDesync->getValueF32();
    ball.setTiming(timing);
    requestRefresh();
}

// ---------------------------------------------------------------------------
// dancers
// ---------------------------------------------------------------------------

void ALPanelDanceBall::showFeedbackAdded(U32 count)
{
    if (count > 0)
    {
        setMessageFormat(getString("msg_added_dancers"), "[COUNT]",
                         llformat("%u", count));
    }
    else
    {
        setMessage(getString("msg_no_dancers_added"));
    }
    requestRefresh();
}

void ALPanelDanceBall::onAddSelected()
{
    ALDanceBall& ball = ALDanceBall::instance();
    LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
    std::vector<LLVOAvatar*> avatars;
    auto consider = [&avatars](LLViewerObject* object)
    {
        if (LLVOAvatar* avatar = avatarFromObject(object))
        {
            if (std::find(avatars.begin(), avatars.end(), avatar) == avatars.end())
            {
                avatars.push_back(avatar);
            }
        }
    };
    if (selection.notNull())
    {
        for (LLObjectSelection::valid_iterator iter = selection->valid_begin();
             iter != selection->valid_end(); ++iter)
        {
            if (LLSelectNode* node = *iter)
            {
                consider(node->getObject());
            }
        }
        consider(selection->getPrimaryObject());
    }
    if (avatars.empty())
    {
        setMessage(getString("msg_nothing_selected"));
        return;
    }
    U32 added = 0;
    bool needs_me = false;
    for (LLVOAvatar* avatar : avatars)
    {
        const Kind kind = kindForAvatar(avatar);
        if (kind == Kind::SELF && !ball.includeSelf())
        {
            needs_me = true;
            continue;
        }
        ball.include(rosterIdFor(avatar, kind));
        added += addAvatarToBall(avatar) ? 1U : 0U;
    }
    if (added == 0 && needs_me)
    {
        setMessage(getString("msg_include_me_first"));
        return;
    }
    showFeedbackAdded(added);
}

void ALPanelDanceBall::onAddCast()
{
    showFeedbackAdded(ALDanceBall::instance().addCast());
}

void ALPanelDanceBall::onAddGhosts()
{
    ALDanceBall& ball = ALDanceBall::instance();
    U32 added = 0;
    for (const ALGhostStudio::Instance& instance :
         ALGhostStudio::instance().getInstances())
    {
        if (instance.mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            added += ball.addDancer(instance.mId, Kind::GHOST) ? 1U : 0U;
        }
    }
    showFeedbackAdded(added);
}

void ALPanelDanceBall::onAddNearby()
{
    const F32 radius = mRadius->getValueF32();
    const ALDanceBall::AutoJoinFocus focus =
        static_cast<ALDanceBall::AutoJoinFocus>(mFocus->getValue().asInteger());
    showFeedbackAdded(ALDanceBall::instance().addNearby(radius, focus));
}

void ALPanelDanceBall::onRemove()
{
    const std::vector<LLScrollListItem*> selected = mDancerList->getAllSelected();
    if (selected.empty())
    {
        setMessage(getString("msg_select_dancers"));
        return;
    }
    ALDanceBall& ball = ALDanceBall::instance();
    U32 removed = 0;
    for (LLScrollListItem* item : selected)
    {
        removed += ball.removeDancer(item->getValue().asUUID()) ? 1U : 0U;
    }
    setMessageFormat(getString("msg_removed_dancers"), "[COUNT]",
                     llformat("%u", removed));
    requestRefresh();
}

void ALPanelDanceBall::onExclude()
{
    const std::vector<LLScrollListItem*> selected = mDancerList->getAllSelected();
    if (selected.empty())
    {
        setMessage(getString("msg_select_dancers"));
        return;
    }
    std::vector<LLUUID> ids;
    for (LLScrollListItem* item : selected)
    {
        ids.push_back(item->getValue().asUUID());
    }
    ALDanceBall& ball = ALDanceBall::instance();
    for (const LLUUID& id : ids)
    {
        ball.exclude(id);
    }
    setMessageFormat(getString("msg_excluded_dancers"), "[COUNT]",
                     llformat("%d", static_cast<S32>(ids.size())));
    requestRefresh();
}

void ALPanelDanceBall::onClearExclusions()
{
    ALDanceBall& ball = ALDanceBall::instance();
    const std::vector<LLUUID> excluded(ball.exclusions().begin(),
                                       ball.exclusions().end());
    for (const LLUUID& id : excluded)
    {
        ball.include(id);
    }
    setMessageFormat(getString("msg_cleared_exclusions"), "[COUNT]",
                     llformat("%d", static_cast<S32>(excluded.size())));
    requestRefresh();
}

void ALPanelDanceBall::onAutoJoinChanged()
{
    if (mRefreshing)
    {
        return;
    }
    ALDanceBall::instance().setAutoJoin(
        mAutoJoin->get(), mRadius->getValueF32(),
        static_cast<ALDanceBall::AutoJoinFocus>(mFocus->getValue().asInteger()));
    requestRefresh();
}

void ALPanelDanceBall::onIncludeSeated()
{
    ALDanceBall::instance().setIncludeSeated(mIncludeSeated->get());
    requestRefresh();
}

void ALPanelDanceBall::onIncludeMe()
{
    ALDanceBall& ball = ALDanceBall::instance();
    const bool include = mIncludeMe->get();
    ball.setIncludeSelf(include);
    if (include)
    {
        ball.addDancer(gAgentID, Kind::SELF);
    }
    requestRefresh();
}

// ---------------------------------------------------------------------------
// playlist
// ---------------------------------------------------------------------------

void ALPanelDanceBall::onFromExplorer()
{
    AnimationExplorer* explorer =
        LLFloaterReg::findTypedInstance<AnimationExplorer>("animation_explorer");
    if (!explorer)
    {
        setMessage(getString("msg_explorer_closed"));
        return;
    }
    const LLUUID id = explorer->getSelectedAnimId();
    if (id.isNull())
    {
        setMessage(getString("msg_explorer_none"));
        return;
    }
    if (addAnimationAsset(id))
    {
        setMessageFormat(getString("msg_items_added"), "[COUNT]", "1");
    }
    else
    {
        setMessage(getString("msg_item_duplicate"));
    }
    requestRefresh();
}

void ALPanelDanceBall::onPasteUUID()
{
    LLWString pasted;
    if (!LLClipboard::instance().pasteFromClipboard(pasted))
    {
        setMessage(getString("msg_no_uuid"));
        return;
    }
    const std::string text = wstring_to_utf8str(pasted);
    // accept whitespace / comma / semicolon separated UUIDs
    std::string token;
    S32 added = 0;
    auto flush = [&]()
    {
        const std::string candidate = trimmed(token);
        token.clear();
        if (!candidate.empty() && LLUUID::validate(candidate))
        {
            LLUUID id;
            id.set(candidate, false);
            added += addAnimationAsset(id) ? 1 : 0;
        }
    };
    for (char c : text)
    {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';')
        {
            flush();
        }
        else
        {
            token.push_back(c);
        }
    }
    flush();
    if (added > 0)
    {
        setMessageFormat(getString("msg_items_added"), "[COUNT]",
                         llformat("%d", added));
    }
    else
    {
        setMessage(getString("msg_no_uuid"));
    }
    requestRefresh();
}

void ALPanelDanceBall::onPlaylistRemove()
{
    const S32 index = selectedPlaylistIndex();
    if (index < 0)
    {
        setMessage(getString("msg_select_item"));
        return;
    }
    mPendingPlaylistSelect = index;
    ALDanceBall::instance().removeAnimation(static_cast<std::size_t>(index));
    requestRefresh();
}

void ALPanelDanceBall::onPlaylistClear()
{
    ALDanceBall::instance().clearPlaylist();
    mPendingPlaylistSelect = -1;
    requestRefresh();
}

void ALPanelDanceBall::onPlaylistUp()
{
    const S32 index = selectedPlaylistIndex();
    if (index <= 0)
    {
        return;
    }
    if (ALDanceBall::instance().moveAnimation(static_cast<std::size_t>(index),
                                              static_cast<std::size_t>(index - 1)))
    {
        mPendingPlaylistSelect = index - 1;
    }
    requestRefresh();
}

void ALPanelDanceBall::onPlaylistDown()
{
    const S32 index = selectedPlaylistIndex();
    if (index < 0)
    {
        return;
    }
    if (ALDanceBall::instance().moveAnimation(static_cast<std::size_t>(index),
                                              static_cast<std::size_t>(index + 1)))
    {
        mPendingPlaylistSelect = index + 1;
    }
    requestRefresh();
}

void ALPanelDanceBall::onPlaylistRename()
{
    const S32 index = selectedPlaylistIndex();
    if (index < 0)
    {
        setMessage(getString("msg_select_item"));
        return;
    }
    const std::string label = trimmed(mRenameEdit->getText());
    if (label.empty())
    {
        return;
    }
    if (ALDanceBall::instance().renameAnimation(static_cast<std::size_t>(index), label))
    {
        setMessage(getString("msg_renamed"));
    }
    requestRefresh();
}

void ALPanelDanceBall::onPlaylistSelection()
{
    if (mRefreshing)
    {
        return;
    }
    const S32 index = selectedPlaylistIndex();
    const std::vector<ALDanceBall::PlaylistEntry>& playlist =
        ALDanceBall::instance().playlist();
    if (index >= 0 && static_cast<std::size_t>(index) < playlist.size())
    {
        mRenameEdit->setText(playlist[static_cast<std::size_t>(index)].mLabel);
    }
}

// Empty selection = the whole roster (Play on all, or nothing selected).
bool ALPanelDanceBall::playRowOnDancers(std::size_t index,
                                        const std::vector<LLUUID>& dancer_ids)
{
    ALDanceBall& ball = ALDanceBall::instance();
    if (dancer_ids.empty())
    {
        return ball.play(index);
    }
    const std::set<LLUUID> selected(dancer_ids.begin(), dancer_ids.end());
    return ball.play(index, selected);
}

// The item's label while the dancer is playing; the state text otherwise.
std::string ALPanelDanceBall::nowPlayingText(
    const ALDanceBall::Dancer& dancer) const
{
    if (dancer.mState == State::PLAYING)
    {
        const std::string label = ALDanceBall::instance().currentLabel(dancer.mId);
        if (!label.empty())
        {
            return label;
        }
    }
    return stateText(dancer.mState);
}

// Generic failure key. onImport() shows the engine's reason in place of this
// when importErrorReason() is non-empty.
const char* ALPanelDanceBall::importFailureKey()
{
    return "msg_import_fail";
}

bool ALPanelDanceBall::addAvatarToBall(LLVOAvatar* avatar)
{
    // the engine classifies the id itself and canonicalizes animesh/ghost ids
    return ALDanceBall::instance().addDancer(avatar->getID());
}

void ALPanelDanceBall::onPlaySelected()
{
    const S32 index = selectedPlaylistIndex();
    if (index < 0)
    {
        setMessage(getString("msg_select_item"));
        return;
    }
    ALDanceBall& ball = ALDanceBall::instance();
    if (ball.roster().empty())
    {
        setMessage(getString("msg_need_dancers"));
        return;
    }
    // Selected dancers (Dancers tab); none selected = the whole roster.
    std::vector<LLUUID> dancers;
    for (LLScrollListItem* item : mDancerList->getAllSelected())
    {
        dancers.push_back(item->getValue().asUUID());
    }
    if (!ball.enabled())
    {
        ball.setEnabled(true);
    }
    if (playRowOnDancers(static_cast<std::size_t>(index), dancers))
    {
        const auto& playlist = ball.playlist();
        setMessageFormat(getString("msg_playing_item"), "[NAME]",
                         playlist[static_cast<std::size_t>(index)].mLabel);
    }
    requestRefresh();
}

void ALPanelDanceBall::onPlayAll()
{
    const S32 index = selectedPlaylistIndex();
    if (index < 0)
    {
        setMessage(getString("msg_select_item"));
        return;
    }
    ALDanceBall& ball = ALDanceBall::instance();
    if (ball.roster().empty())
    {
        setMessage(getString("msg_need_dancers"));
        return;
    }
    // Everyone dances the same item: switch to Same-for-all, then punch it.
    ALDanceBall::Timing timing = ball.timing();
    if (timing.mMode != ALDanceBallModel::PlaylistMode::SAME_FOR_ALL)
    {
        timing.mMode = ALDanceBallModel::PlaylistMode::SAME_FOR_ALL;
        ball.setTiming(timing);
    }
    if (!ball.enabled())
    {
        ball.setEnabled(true);
    }
    if (playRowOnDancers(static_cast<std::size_t>(index), std::vector<LLUUID>()))
    {
        const auto& playlist = ball.playlist();
        setMessageFormat(getString("msg_playing_item"), "[NAME]",
                         playlist[static_cast<std::size_t>(index)].mLabel);
    }
    requestRefresh();
}

void ALPanelDanceBall::onModeChanged()
{
    ALDanceBall& ball = ALDanceBall::instance();
    ALDanceBall::Timing timing = ball.timing();
    const S32 mode = std::clamp(mMode->getValue().asInteger(), 0, 2);
    timing.mMode = static_cast<ALDanceBallModel::PlaylistMode>(mode);
    ball.setTiming(timing);
    requestRefresh();
}

void ALPanelDanceBall::onAutoAdvanceChanged()
{
    ALDanceBall& ball = ALDanceBall::instance();
    ALDanceBall::Timing timing = ball.timing();
    timing.mAutoAdvance = mAutoAdvance->get();
    timing.mAdvanceSeconds = mAdvInterval->getValue().asReal();
    timing.mAdvanceJitterSeconds = mAdvJitter->getValue().asReal();
    ball.setTiming(timing);
    requestRefresh();
}

// ---------------------------------------------------------------------------
// timing / cinema
// ---------------------------------------------------------------------------

void ALPanelDanceBall::onTimingChanged(bool speed_edited)
{
    ALDanceBall& ball = ALDanceBall::instance();
    ALDanceBall::Timing timing = ball.timing();
    timing.mDesync = mTDesync->getValueF32();
    if (speed_edited)
    {
        timing.mSpeed = mTSpeed->getValueF32();
    }
    timing.mSpeedJitter = mTSpeedJitter->getValueF32();
    timing.mCrossfadeSeconds = mTCrossfade->getValueF32();
    timing.mPriority = mTPriority->getValue().asInteger();
    timing.mDanceDrivesFace = mTDanceDrivesFace->get();
    ball.setTiming(timing);
    requestRefresh();
}

void ALPanelDanceBall::onReshuffle()
{
    ALDanceBall::instance().reshuffle();
    setMessage(getString("msg_reshuffled"));
    requestRefresh();
}

void ALPanelDanceBall::onWaveChanged()
{
    ALDanceBall::Wave wave;
    wave.mEnabled = mWaveEnable->get();
    wave.mFocus = static_cast<ALDanceBall::AutoJoinFocus>(
        std::clamp(mWaveFocus->getValue().asInteger(), 0, 2));
    wave.mSpreadMetersPerSecond = mWaveSpeed->getValue().asReal();
    wave.mDirection = static_cast<ALDanceBallModel::WaveDirection>(
        std::clamp(mWaveDir->getValue().asInteger(), 0, 1));
    wave.mApply = static_cast<ALDanceBallModel::WaveApply>(
        std::clamp(mWaveApply->getValue().asInteger(), 0, 2));
    ALDanceBall::instance().setWave(wave);
    requestRefresh();
}

void ALPanelDanceBall::onRampGo()
{
    ALDanceBall::instance().rampSpeed(mRampTarget->getValue().asReal(),
                                      mRampSeconds->getValue().asReal());
    setMessage(getString("msg_ramping"));
    requestRefresh();
}

void ALPanelDanceBall::onExport()
{
    const LLHandle<ALPanelDanceBall> handle = getDerivedHandle<ALPanelDanceBall>();
    const auto callback =
        [handle](const std::vector<std::string>& files,
                 LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
    {
        if (files.empty())
        {
            return;
        }
        // the engine singleton outlives the floater: never capture `this`
        bool ok = false;
        {
            llofstream file(files[0].c_str());
            if (file.is_open())
            {
                LLSDSerialize::toPrettyXML(
                    ALDanceBall::instance().exportSession(), file);
                ok = file.good();
            }
        }
        if (ALPanelDanceBall* panel = handle.get())
        {
            panel->setMessage(panel->getString(
                ok ? "msg_export_ok" : "msg_export_fail"));
        }
    };
    LLFilePickerReplyThread::startPicker(
        callback, LLFilePicker::FFSAVE_XML, getString("export_default_name"));
}

void ALPanelDanceBall::onImport()
{
    const LLHandle<ALPanelDanceBall> handle = getDerivedHandle<ALPanelDanceBall>();
    const auto callback =
        [handle](const std::vector<std::string>& files,
                 LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
    {
        if (files.empty())
        {
            return;
        }
        const char* result = "msg_import_open_fail";
        std::string reason;   // the engine's reason, set only when importSession() rejected the data
        {
            llifstream file(files[0].c_str());
            if (file.is_open())
            {
                file.seekg(0, std::ios::end);
                const std::streamoff size = file.tellg();
                file.seekg(0, std::ios::beg);
                if (size <= 0 || size > MAX_SESSION_FILE_BYTES)
                {
                    result = "msg_import_fail";
                }
                else
                {
                    LLSD data;
                    if (LLSDSerialize::fromXML(data, file) ==
                            LLSDParser::PARSE_FAILURE ||
                        !data.isMap())
                    {
                        result = importFailureKey();
                    }
                    else if (!ALDanceBall::instance().importSession(data))
                    {
                        result = importFailureKey();
                        reason = ALDanceBall::instance().importErrorReason();
                    }
                    else
                    {
                        result = "msg_import_ok";
                    }
                }
            }
        }
        if (ALPanelDanceBall* panel = handle.get())
        {
            if (!reason.empty())
            {
                LLStringUtil::format_map_t args;
                args["[REASON]"] = reason;
                panel->setMessage(panel->formatString(
                    panel->getString("msg_import_fail_reason"), args));
            }
            else
            {
                panel->setMessage(panel->getString(result));
            }
            panel->requestRefresh();
        }
    };
    LLFilePickerReplyThread::startPicker(callback, LLFilePicker::FFLOAD_XML, false);
}

// ---------------------------------------------------------------------------
// drag and drop
// ---------------------------------------------------------------------------

bool ALPanelDanceBall::addDroppedAnimation(const LLUUID& asset_id,
                                           const std::string& name)
{
    if (asset_id.isNull())
    {
        return false;
    }
    if (!addAnimationAsset(asset_id, name))
    {
        setMessage(getString("msg_item_duplicate"));
        return false;
    }
    setMessageFormat(getString("msg_item_added"), "[NAME]", name);
    requestRefresh();
    return true;
}

bool ALPanelDanceBall::handleDragAndDrop(
    S32 x, S32 y, MASK mask, bool drop, EDragAndDropType cargo_type,
    void* cargo_data, EAcceptance* accept, std::string& tooltip_msg)
{
    if (cargo_type != DAD_ANIMATION && cargo_type != DAD_CATEGORY)
    {
        return LLPanel::handleDragAndDrop(
            x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
    }

    // DAD_ANIMATION: one call per dropped item. DAD_CATEGORY: a folder of
    // dances, every animation inside it (any depth) is added on drop.
    *accept = ACCEPT_YES_MULTI;
    if (!drop || !cargo_data)
    {
        return true;
    }

    if (cargo_type == DAD_ANIMATION)
    {
        LLInventoryItem* item = static_cast<LLInventoryItem*>(cargo_data);
        addDroppedAnimation(item->getAssetUUID(), item->getName());
        return true;
    }

    LLInventoryCategory* category = static_cast<LLInventoryCategory*>(cargo_data);
    LLInventoryModel::cat_array_t categories;
    LLInventoryModel::item_array_t items;
    gInventory.collectDescendents(category->getUUID(), categories, items, false);
    U32 added = 0;
    std::size_t seen = 0;
    for (const LLPointer<LLViewerInventoryItem>& item : items)
    {
        if (item.isNull() || item->getType() != LLAssetType::AT_ANIMATION)
        {
            continue;
        }
        if (++seen > MAX_DROP_ANIMATIONS)
        {
            break;
        }
        if (addAnimationAsset(item->getAssetUUID(), item->getName()))
        {
            ++added;
        }
    }
    if (added > 0)
    {
        setMessageFormat(getString("msg_items_added"), "[COUNT]",
                         llformat("%u", added));
    }
    else
    {
        setMessage(getString("msg_item_duplicate"));
    }
    requestRefresh();
    return true;
}
