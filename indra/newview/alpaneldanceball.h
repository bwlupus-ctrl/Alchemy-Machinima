/**
 * @file alpaneldanceball.h
 * @brief Dance Ball control panel (hosted by floater_dance_ball.xml).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Mirrors the session-local ALDanceBall engine (aldanceball.h). Everything is
 * client-side: nothing the panel does is sent to a simulator.
 *
 * UI RULES (CLAUDE.md "UI panels must not do work in draw()"):
 *  - draw() only compares the engine's U64 revisions (ALRevisionGate), applies
 *    the diff-guarded *IfChanged writes, and updates volatile list cells at
 *    most 2 Hz while the list is on screen.
 *  - Lists are rebuilt only when the structural revision moves.
 *  - Widget pointers are cached in postBuild(); no getChild in draw().
 *  - ALScrollFit wraps and fits the tab pages (installForTabs).
 */

#ifndef AL_ALPANELDANCEBALL_H
#define AL_ALPANELDANCEBALL_H

#include "aldanceball.h"
#include "alscrollfit.h"
#include "aluirefresh.h"
#include "llpanel.h"

#include <string>
#include <unordered_map>
#include <vector>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTabContainer;
class LLTextBox;
class LLViewerObject;
class LLVOAvatar;

class ALPanelDanceBall final : public LLPanel
{
public:
    ALPanelDanceBall();
    ~ALPanelDanceBall() override;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

    // Inventory drag and drop: DAD_ANIMATION (multi) and DAD_CATEGORY
    // (every animation inside the folder).
    bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
                           EDragAndDropType cargo_type, void* cargo_data,
                           EAcceptance* accept,
                           std::string& tooltip_msg) override;

    // ---- shared entry points (avatar right-click menu, Animation Explorer)
    // Toggle an avatar's roster membership. Shows the floater when it adds.
    static void toggleAvatar(LLVOAvatar* avatar);
    static bool isAvatarDancer(LLVOAvatar* avatar);
    // Playlist add used by the Animation Explorer. Returns true when added.
    static bool addAnimationAsset(const LLUUID& asset_id,
                                  const std::string& label = std::string());

private:
    // A cached copy of what each list row currently shows, so the 2 Hz cell
    // update writes only the cells that differ.
    struct DancerRow
    {
        std::string mShortId;
        std::string mOn;
        std::string mName;
        std::string mKind;
        std::string mNow;
        std::string mDist;
    };
    struct PlaylistRow
    {
        std::string mName;
        std::string mLength;
        std::string mLoop;
        std::string mStatus;
    };

    static ALDanceBall::DancerKind kindForAvatar(LLVOAvatar* avatar);
    static LLUUID rosterIdFor(LLVOAvatar* avatar,
                              ALDanceBall::DancerKind kind);
    static LLVOAvatar* avatarFromObject(LLViewerObject* object);
    LLVOAvatar* resolveDancer(const ALDanceBall::Dancer& dancer) const;

    // ---- text helpers (strings come from the XML, cached in postBuild)
    std::string kindText(ALDanceBall::DancerKind kind) const;
    std::string stateText(ALDanceBall::DancerState state) const;
    static bool playRowOnDancers(std::size_t index,
                                 const std::vector<LLUUID>& dancer_ids);
    std::string nowPlayingText(const ALDanceBall::Dancer& dancer) const;
    static const char* importFailureKey();
    static bool addAvatarToBall(LLVOAvatar* avatar);
    std::string dancerName(const ALDanceBall::Dancer& dancer,
                           LLVOAvatar* avatar,
                           const std::string& short_id) const;
    std::string dancerDistance(const ALDanceBall::Dancer& dancer,
                               LLVOAvatar* avatar) const;
    std::string itemStatusText(ALDanceBall::AssetStatus status) const;
    std::string formatString(const std::string& tmpl,
                             const LLStringUtil::format_map_t& args) const;
    void setMessage(const std::string& text);
    void setMessageFormat(const std::string& tmpl, const char* key,
                          const std::string& value);

    // ---- refresh (event or gate driven, never unconditional per frame)
    void requestRefresh();
    void refreshHeader(const ALDanceBall& ball);
    void rebuildDancerList(const ALDanceBall& ball);
    void updateDancerCells(const ALDanceBall& ball);
    void rebuildPlaylistList(const ALDanceBall& ball);
    void updatePlaylistCells(const ALDanceBall& ball);
    void refreshDancerControls(const ALDanceBall& ball);
    void refreshPlaylistControls(const ALDanceBall& ball);
    void refreshTimingControls(const ALDanceBall& ball);
    void refreshLiveSpeed(const ALDanceBall& ball);
    void refreshCinemaControls(const ALDanceBall& ball);
    S32 selectedPlaylistIndex() const;

    // ---- header / transport
    void onToggleOn();
    void onPrevious();
    void onNext();
    void onFreeze();
    void onShuffle();
    void onStopAll();
    void onHeaderDesync();

    // ---- dancers
    void onAddSelected();
    void onAddCast();
    void onAddGhosts();
    void onAddNearby();
    void onRemove();
    void onExclude();
    void onClearExclusions();
    void onAutoJoinChanged();
    void onIncludeSeated();
    void onIncludeMe();

    // ---- playlist
    void onFromExplorer();
    void onPasteUUID();
    void onPlaylistRemove();
    void onPlaylistClear();
    void onPlaylistUp();
    void onPlaylistDown();
    void onPlaylistRename();
    void onPlaylistSelection();
    void onPlaySelected();
    void onPlayAll();
    void onModeChanged();
    void onAutoAdvanceChanged();

    // ---- timing
    void onTimingChanged(bool speed_edited);
    void onReshuffle();

    // ---- cinema
    void onWaveChanged();
    void onRampGo();
    void onExport();
    void onImport();

    bool addDroppedAnimation(const LLUUID& asset_id, const std::string& name);
    void showFeedbackAdded(U32 count);

    // cached widgets (postBuild)
    LLButton*         mOnBtn = nullptr;
    LLButton*         mPrevBtn = nullptr;
    LLButton*         mNextBtn = nullptr;
    LLButton*         mFreezeBtn = nullptr;
    LLButton*         mShuffleBtn = nullptr;
    LLButton*         mStopAllBtn = nullptr;
    LLSliderCtrl*     mHeaderDesync = nullptr;
    LLTextBox*        mStatusText = nullptr;
    LLTextBox*        mMessageText = nullptr;
    LLTabContainer*   mTabs = nullptr;

    LLScrollListCtrl* mDancerList = nullptr;
    LLButton*         mAddSelectedBtn = nullptr;
    LLButton*         mAddCastBtn = nullptr;
    LLButton*         mAddGhostsBtn = nullptr;
    LLButton*         mRemoveBtn = nullptr;
    LLButton*         mExcludeBtn = nullptr;
    LLButton*         mAddNearbyBtn = nullptr;
    LLButton*         mClearExclusionsBtn = nullptr;
    LLSpinCtrl*       mRadius = nullptr;
    LLComboBox*       mFocus = nullptr;
    LLCheckBoxCtrl*   mAutoJoin = nullptr;
    LLCheckBoxCtrl*   mIncludeSeated = nullptr;
    LLCheckBoxCtrl*   mIncludeMe = nullptr;

    LLScrollListCtrl* mPlaylistList = nullptr;
    LLButton*         mFromExplorerBtn = nullptr;
    LLButton*         mPasteBtn = nullptr;
    LLButton*         mPlRemoveBtn = nullptr;
    LLButton*         mPlClearBtn = nullptr;
    LLButton*         mPlUpBtn = nullptr;
    LLButton*         mPlDownBtn = nullptr;
    LLButton*         mPlRenameBtn = nullptr;
    LLLineEditor*     mRenameEdit = nullptr;
    LLButton*         mPlaySelectedBtn = nullptr;
    LLButton*         mPlayAllBtn = nullptr;
    LLComboBox*       mMode = nullptr;
    LLCheckBoxCtrl*   mAutoAdvance = nullptr;
    LLSpinCtrl*       mAdvInterval = nullptr;
    LLSpinCtrl*       mAdvJitter = nullptr;

    LLSliderCtrl*     mTDesync = nullptr;
    LLButton*         mReshuffleBtn = nullptr;
    LLSliderCtrl*     mTSpeed = nullptr;
    LLSliderCtrl*     mTSpeedJitter = nullptr;
    LLSliderCtrl*     mTCrossfade = nullptr;
    LLComboBox*       mTPriority = nullptr;
    LLCheckBoxCtrl*   mTDanceDrivesFace = nullptr;

    LLCheckBoxCtrl*   mWaveEnable = nullptr;
    LLComboBox*       mWaveFocus = nullptr;
    LLSliderCtrl*     mWaveSpeed = nullptr;
    LLComboBox*       mWaveDir = nullptr;
    LLComboBox*       mWaveApply = nullptr;
    LLSpinCtrl*       mRampTarget = nullptr;
    LLSpinCtrl*       mRampSeconds = nullptr;
    LLButton*         mRampGoBtn = nullptr;
    LLButton*         mExportBtn = nullptr;
    LLButton*         mImportBtn = nullptr;

    // strings cached once from the XML (never getString in draw)
    std::string mStrKind[4];
    std::string mStrState[11];
    std::string mStrOnYes, mStrOnNo, mStrNoValue, mStrNameLoading;
    std::string mStrNameAnimesh, mStrNameGhost, mStrDistFormat;
    std::string mStrLengthFormat, mStrLoopYes, mStrLoopNo;
    std::string mStrItem[3];
    std::string mStrStatusOff, mStrStatusOn, mStrStatusFrozen, mStrStatusPlaying;
    std::string mStrMode[3];
    std::string mStrSyncSynced, mStrSyncDesync;

    // gates: each watches the engine revisions its area depends on
    ALRevisionGate mHeaderGate;
    ALRevisionGate mDancerStructGate;
    ALRevisionGate mPlaylistStructGate;
    ALRevisionGate mPlaylistCellGate;
    ALRevisionGate mDancerControlGate;
    ALRevisionGate mPlaylistControlGate;
    ALRevisionGate mTimingGate;
    ALRevisionGate mCinemaGate;
    ALLiveReadoutClock mVolatileClock{2.f};   // the single 2 Hz refresh cap

    std::unordered_map<LLUUID, DancerRow> mDancerRows;
    std::vector<PlaylistRow> mPlaylistRows;
    S32 mPendingPlaylistSelect = -1;

    ALScrollFit::Controller mScrollFit;
    bool mRefreshing = false;
    bool mLiveSpeedShown = false;   // the speed slider currently shows a live ramp value
};

#endif // AL_ALPANELDANCEBALL_H
