/**
 * @file alvcammonitorstrip.h
 * @brief Fixed Director VCam monitor, selector, pager, and face-list widgets.
 */

#ifndef AL_VCAM_MONITOR_STRIP_H
#define AL_VCAM_MONITOR_STRIP_H

#include "alvcampagemodel.h"
#include "alflowgrid.h"
#include "llui.h"
#include "lluiimage.h"
#include "lluictrl.h"
#include "llprismpreview.h"

#include <array>
#include <vector>

class LLFontGL;
class LLButton;
class LLComboBox;
class LLTextBox;

class ALVCamMonitorHeader : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params> { Params() = default; };
    void setState(bool compact, bool previews, bool info_overlay, F32 speed);
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamMonitorHeader(const Params& p);
private:
    bool mCompact = false;
    bool mPreviews = true;
    bool mInfoOverlay = false;
    F32 mSpeed = 1.f;
    const LLFontGL* mFont = nullptr;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
    LLColor4 mAccentColor;
    LLColor4 mBorderColor;
};

// Manifest-only placeholder for the "vcam_monitor_tile" XUI tag. It needs its OWN Params type:
// LLUICtrlFactory::registerWidget deliberately crashes if one Params type is registered under
// two tag names (lluictrlfactory.cpp:262-267).
class ALVCamMonitorTileInfo : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params> { Params() = default; };
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamMonitorTileInfo(const Params& p) : LLUICtrl(p) {}
};

class ALVCamMonitorStrip : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Params() = default;
    };
    struct Tile
    {
        LLPrismLens::CaptureHandle mCapture;
        LLPrismPreviewService::TilePublication mPublication;
        std::string mLabel;
        std::string mStatus;
        bool mNoSignal = false;
        bool mNoSignalWhite = false;
        LLPrismLens::EActivityState mActivity = LLPrismLens::EActivityState::IDLE;
        F32 mObservedFps = 0.f;
        F32 mTargetFps = 0.f;
        F32 mPreviewFps = 0.f;
        U32 mFaceCount = 0;
        U32 mMonitorDisplayIndex = 0;
        bool mEditing = false;
        bool mMasterEnabled = false;
        bool mPreviewsEnabled = false;
        bool mMonitorShowsDisplay = false;
        bool mMonitorDisplayVirtual = false;
        bool mVisible = false;
        bool mMedia = false;
        bool mGate = false;
    };

    void setTiles(const std::array<Tile, LLPrismLens::MAX_CAPTURES>& tiles,
                  U32 count, bool compact, bool info_overlay, U32 large_page);
    const std::string& previewCostText(bool previews_enabled, F32 preview_fps);
    const std::string& monitorFooterText(bool master_enabled, bool previews_enabled,
                                         U32 monitor_count, U32 preview_only,
                                         F32 preview_fps);
    void setNewFaceFit(LLPrismLens::EFitMode fit) { mNewFaceFit = fit; }
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;

    static bool beginDragFromTile(
        const LLPrismLens::CaptureHandle& capture,
        LLPrismLens::EFitMode new_face_fit = LLPrismLens::EFitMode::FIT,
        std::string* reason = nullptr);
    static bool beginDragFromTile(
        const LLUUID& capture_id,
        LLPrismLens::EFitMode new_face_fit = LLPrismLens::EFitMode::FIT,
        std::string* reason = nullptr);
    static LLPrismLens::EFitMode activeDragFitMode();
    static bool handleDisplayRowDrop(
        const LLPrismLens::DisplayHandle& display, bool drop,
        EDragAndDropType cargo_type, void* cargo_data, EAcceptance* accept,
        std::string& tooltip_msg);

protected:
    friend class LLUICtrlFactory;
    explicit ALVCamMonitorStrip(const Params& p);

private:
    struct TileTextCache
    {
        std::string mName;
        std::string mStatus;
        std::string mLabelInput;
        bool mMedia = false;
        bool mGate = false;
        bool mValid = false;
    };
    void updateTileText(U32 slot, const Tile& tile, bool info_overlay, bool force_info);
    S32 tileAt(S32 x, S32 y) const;
    LLRect tileRect(S32 visible_index) const;
    std::array<Tile, LLPrismLens::MAX_CAPTURES> mTiles;
    std::array<TileTextCache, LLPrismLens::MAX_CAPTURES> mTileText;
    U32 mCount = 0;
    U32 mLargePage = 0;
    bool mCompact = false;
    bool mInfoOverlay = false;
    LLPrismLens::EFitMode mNewFaceFit = LLPrismLens::EFitMode::FIT;
    const LLFontGL* mFont = nullptr;
    LLColor4 mBorderColor;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
    LLPrismPreviewService* mPreviewService = nullptr;
    S32 mMouseDownSlot = -1;
    std::string mPreviewCostText;
    std::string mMonitorFooterText;
    S32 mPreviewCostTenths = -1;
    S32 mMonitorFooterTenths = -1;
    U32 mMonitorFooterCount = 0;
    U32 mMonitorFooterPreviewOnly = 0;
    bool mPreviewCostEnabled = false;
    bool mMonitorFooterMasterEnabled = false;
    bool mMonitorFooterPreviewsEnabled = false;
};

class ALVCamMonitorPager : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Params() = default;
    };
    void setPage(U32 page, U32 count);
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamMonitorPager(const Params& p);
private:
    U32 mPage = 0;
    U32 mCount = 0;
    std::string mLabel;
    const LLFontGL* mFont = nullptr;
    LLColor4 mTextColor;
};

class ALVCamCameraSelector : public LLUICtrl, public ALFlowGridItem
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Params() = default;
    };
    struct Item
    {
        LLPrismLens::CaptureHandle mHandle;
        std::string mName;
        U32 mSlot = 0;
        bool mLive = false;
        bool mPreview = false;
        bool mGate = false;
    };
    bool postBuild() override;
    void setItems(const std::vector<Item>& items, const LLPrismLens::CaptureHandle& selected);
    void setActionsEnabled(bool can_add, bool can_remove, bool media_enabled);
    U32 itemCount() const { return static_cast<U32>(mItems.size()); }
    bool hasSelection() const { return !mSelected.mId.isNull(); }
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
    S32 heightForWidth(S32 width) const override;
    void draw() override;
    bool handleKeyHere(KEY key, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamCameraSelector(const Params& p);
private:
    struct Layout
    {
        LLRect mLabel, mAdd, mRemove;
        std::vector<LLRect> mChips;
        S32 mHeight = 24;
    };
    Layout layoutForWidth(S32 width) const;
    void requestLayout();
    void selectChip(S32 index);
    std::vector<Item> mItems;
    // Mutually exclusive, focusable radio chips. The selector remains the
    // commit/probe target; child buttons add native tooltips and keyboard focus.
    std::vector<LLButton*> mChips;
    Layout mLayout;
    LLUIImagePtr mChipImage;
    LLPrismLens::CaptureHandle mSelected;
    const LLFontGL* mFont = nullptr;
    const LLFontGL* mSelectedFont = nullptr;
    LLUIColor mLiveColor;
    LLComboBox* mAdd = nullptr;
    LLTextBox* mLabel = nullptr;
    LLButton* mRemove = nullptr;
    bool mMediaOptions = false;
};

class ALVCamFaceList : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Params() = default;
    };
    struct Item
    {
        LLPrismLens::DisplayHandle mHandle;
        std::string mName;
        std::string mMeta;
        bool mRaw = false;
        bool mSelected = false;
    };
    void setItems(const std::vector<Item>& items);
    void setMovePick(bool active) { mMovePick = active; }
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleScrollWheel(S32 x, S32 y, LLScrollDelta delta) override;
    bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
                           EDragAndDropType cargo_type, void* cargo_data,
                           EAcceptance* accept, std::string& tooltip_msg) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamFaceList(const Params& p);
private:
    S32 rowAt(S32 y) const;
    void setScrollOffset(S32 offset);
    std::vector<Item> mItems;
    S32 mScrollOffset = 0;
    bool mMovePick = false;
    const LLFontGL* mFont = nullptr;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
    LLColor4 mAccentColor;
    LLColor4 mBorderColor;
};

class ALVCamTexturePickerList : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params> { Params() = default; };
    struct Item
    {
        LLPrismLens::CaptureHandle mCapture;
        LLPrismPreviewService::TilePublication mPublication;
        std::string mLabel;
    };
    void setItems(const std::array<Item, LLPrismLens::MAX_CAPTURES>& items,
                  U32 count, const LLUUID& selected);
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamTexturePickerList(const Params& p);
private:
    S32 rowAt(S32 y) const;
    std::array<Item, LLPrismLens::MAX_CAPTURES> mItems;
    U32 mCount = 0;
    LLUUID mSelected;
    const LLFontGL* mFont = nullptr;
    LLColor4 mBorderColor;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
    LLColor4 mAccentColor;
    LLPrismPreviewService* mPreviewService = nullptr;
};

class ALVCamProjectorList : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params> { Params() = default; };
    struct Item
    {
        LLPrismLens::ProjectorHandle mHandle;
        std::string mName;
        std::string mMeta;
        bool mSelected = false;
    };
    void setItems(const std::vector<Item>& items);
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleScrollWheel(S32 x, S32 y, LLScrollDelta delta) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamProjectorList(const Params& p);
private:
    S32 rowAt(S32 y) const;
    void setScrollOffset(S32 offset);
    std::vector<Item> mItems;
    S32 mScrollOffset = 0;
    const LLFontGL* mFont = nullptr;
    LLColor4 mBorderColor;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
    LLColor4 mAccentColor;
};

#endif
