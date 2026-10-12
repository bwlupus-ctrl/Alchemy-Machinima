/**
 * @file alvcammonitorstrip.h
 * @brief Fixed Director VCam monitor, selector, pager, and face-list widgets.
 */

#ifndef AL_VCAM_MONITOR_STRIP_H
#define AL_VCAM_MONITOR_STRIP_H

#include "alvcampagemodel.h"
#include "llui.h"
#include "lluictrl.h"
#include "llprismpreview.h"

#include <array>
#include <vector>

class LLFontGL;
class LLButton;
class LLComboBox;

class ALVCamMonitorHeader : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params> { Params() = default; };
    void setState(bool compact, bool previews, F32 speed);
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamMonitorHeader(const Params& p);
private:
    bool mCompact = false;
    bool mPreviews = true;
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
        std::string mBadge;
        std::string mTimestamp;
        std::string mName;
        std::string mShows;
        std::string mStatus;
        bool mVisible = false;
    };

    void setTiles(const std::array<Tile, LLPrismLens::MAX_CAPTURES>& tiles,
                  U32 count, bool compact, U32 large_page);
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
    S32 tileAt(S32 x, S32 y) const;
    LLRect tileRect(S32 visible_index) const;
    std::array<Tile, LLPrismLens::MAX_CAPTURES> mTiles;
    U32 mCount = 0;
    U32 mLargePage = 0;
    bool mCompact = false;
    LLPrismLens::EFitMode mNewFaceFit = LLPrismLens::EFitMode::FIT;
    const LLFontGL* mFont = nullptr;
    LLColor4 mBorderColor;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
    LLPrismPreviewService* mPreviewService = nullptr;
    S32 mMouseDownSlot = -1;
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

class ALVCamCameraSelector : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Params() = default;
    };
    struct Item { LLPrismLens::CaptureHandle mHandle; std::string mLabel; bool mLive = false; };
    bool postBuild() override;
    void setItems(const std::vector<Item>& items, const LLPrismLens::CaptureHandle& selected);
    void setActionsEnabled(bool can_add, bool can_remove);
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamCameraSelector(const Params& p);
private:
    std::vector<Item> mItems;
    LLPrismLens::CaptureHandle mSelected;
    const LLFontGL* mFont = nullptr;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
    LLColor4 mAccentColor;
    LLColor4 mBorderColor;
    LLComboBox* mAdd = nullptr;
    LLButton* mRemove = nullptr;
    S32 mChipWidth = 1;
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
    };
    void setItems(const std::vector<Item>& items);
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALVCamProjectorList(const Params& p);
private:
    S32 rowAt(S32 y) const;
    std::vector<Item> mItems;
    const LLFontGL* mFont = nullptr;
    LLColor4 mBorderColor;
    LLColor4 mTextColor;
    LLColor4 mMutedColor;
};

#endif
