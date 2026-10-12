/**
 * @file alvcammonitorstrip.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "alvcammonitorstrip.h"

#include "alkitstyle.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llfontgl.h"
#include "llrender.h"
#include "llrender2dutils.h"
#include "lltooldraganddrop.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

#include <algorithm>

static LLDefaultChildRegistry::Register<ALVCamMonitorStrip>
    r_vcam_monitor_strip("vcam_monitor_strip");
static LLDefaultChildRegistry::Register<ALVCamMonitorHeader>
    r_vcam_monitor_header("vcam_monitor_header");
static LLDefaultChildRegistry::Register<ALVCamMonitorStrip>
    r_vcam_monitor_tile("vcam_monitor_tile");
static LLDefaultChildRegistry::Register<ALVCamMonitorPager>
    r_vcam_monitor_pager("vcam_monitor_pager");
static LLDefaultChildRegistry::Register<ALVCamCameraSelector>
    r_vcam_camera_selector("vcam_camera_selector");
static LLDefaultChildRegistry::Register<ALVCamFaceList>
    r_vcam_face_list("vcam_face_list");

namespace
{
LLPrismLens::EFitMode sActiveDragFit = LLPrismLens::EFitMode::FIT;
constexpr S32 FACE_ROW_HEIGHT = 32;
constexpr S32 FACE_SCROLLBAR_WIDTH = 10;

bool sameCapture(const LLPrismLens::CaptureHandle& left,
                 const LLPrismLens::CaptureHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

bool currentCapture(const LLUUID& id, LLPrismLens::CaptureHandle& capture,
                    LLPrismLens::ECaptureMode* mode = nullptr)
{
    const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate = snapshot.mCaptures[index];
        if (candidate.mHandle.mId == id)
        {
            capture = candidate.mHandle;
            if (mode) *mode = candidate.mMode;
            return true;
        }
    }
    return false;
}

void drawText(const LLFontGL* font, const std::string& text,
              S32 x, S32 y, const LLColor4& color)
{
    if (font) font->renderUTF8(text, 0, static_cast<F32>(x), static_cast<F32>(y), color, LLFontGL::LEFT,
                              LLFontGL::BASELINE, LLFontGL::NORMAL,
                              LLFontGL::DROP_SHADOW_SOFT, S32_MAX, S32_MAX,
                              nullptr, false);
}
}

ALVCamMonitorHeader::ALVCamMonitorHeader(const Params& p)
    : LLUICtrl(p), mFont(LLFontGL::getFontSansSerifSmall()),
      mTextColor(ALKitStyle::color(ALKitStyle::TEXT).get()),
      mMutedColor(ALKitStyle::color(ALKitStyle::MUTED).get()),
      mAccentColor(ALKitStyle::color(ALKitStyle::ACCENT).get()),
      mBorderColor(ALKitStyle::color(ALKitStyle::CARD_BORDER).get())
{
    setMouseOpaque(true);
}

void ALVCamMonitorHeader::setState(bool compact, bool previews, F32 speed)
{
    mCompact = compact;
    mPreviews = previews;
    mSpeed = speed;
}

void ALVCamMonitorHeader::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    const S32 w = getRect().getWidth();
    const S32 size_right = w * 3 / 10;
    const S32 preview_right = w * 5 / 10;
    const S32 h = getRect().getHeight();
    drawText(mFont, "Size", 3, 8, mMutedColor);
    const S32 size_left = 34;
    const S32 size_width = llmax(1, size_right - size_left);
    for (S32 index = 0; index < 2; ++index)
    {
        LLRect rect(size_left + index * size_width / 2, h,
                    size_left + (index + 1) * size_width / 2 - 2, 0);
        gl_rect_2d(rect, index == static_cast<S32>(mCompact) ? mAccentColor : mBorderColor, false);
        drawText(mFont, index ? "Compact" : "Large", rect.mLeft + 5, 8, mTextColor);
    }
    drawText(mFont, mPreviews ? "Previews  ON" : "Previews  OFF",
             size_right + 7, 8, mPreviews ? mAccentColor : mMutedColor);
    drawText(mFont, "Speed", preview_right + 3, 8, mMutedColor);
    const F32 speeds[] = { .5f, 1.f, 2.f, 5.f };
    const char* labels[] = { "0.5", "1", "2", "5" };
    const S32 speed_left = preview_right + 42;
    const S32 speed_width = llmax(1, w - speed_left);
    for (S32 index = 0; index < 4; ++index)
    {
        LLRect rect(speed_left + index * speed_width / 4, h,
                    speed_left + (index + 1) * speed_width / 4 - 2, 0);
        gl_rect_2d(rect, fabsf(mSpeed - speeds[index]) < .01f ? mAccentColor : mBorderColor, false);
        drawText(mFont, labels[index], rect.mLeft + 5, 8, mTextColor);
    }
    LLUICtrl::draw();
}

bool ALVCamMonitorHeader::handleMouseDown(S32 x, S32, MASK)
{
    const S32 w = getRect().getWidth();
    const S32 size_right = w * 3 / 10;
    const S32 preview_right = w * 5 / 10;
    LLSD action;
    if (x < size_right)
    {
        action["action"] = "size";
        action["value"] = x >= 34 + llmax(1, size_right - 34) / 2;
    }
    else if (x < preview_right)
    {
        action["action"] = "previews";
        action["value"] = !mPreviews;
    }
    else
    {
        const F32 speeds[] = { .5f, 1.f, 2.f, 5.f };
        const S32 speed_left = preview_right + 42;
        if (x < speed_left) return false;
        const S32 index = llclamp((x - speed_left) * 4 / llmax(1, w - speed_left), 0, 3);
        action["action"] = "speed";
        action["value"] = speeds[index];
    }
    setValue(action);
    onCommit();
    return true;
}

ALVCamMonitorStrip::ALVCamMonitorStrip(const Params& p)
    : LLUICtrl(p),
      mFont(LLFontGL::getFontSansSerifSmall()),
      mBorderColor(ALKitStyle::color(ALKitStyle::CARD_BORDER).get()),
      mTextColor(ALKitStyle::color(ALKitStyle::TEXT).get()),
      mMutedColor(ALKitStyle::color(ALKitStyle::MUTED).get()),
      mPreviewService(&LLPrismPreviewService::instance())
{
    setMouseOpaque(true);
}

void ALVCamMonitorStrip::setTiles(
    const std::array<Tile, LLPrismLens::MAX_CAPTURES>& tiles,
    U32 count, bool compact, U32 large_page)
{
    mTiles = tiles;
    mCount = llmin(count, LLPrismLens::MAX_CAPTURES);
    mCompact = compact;
    mLargePage = llmin(large_page, 1u);
}

LLRect ALVCamMonitorStrip::tileRect(S32 visible_index) const
{
    const S32 columns = mCompact ? 8 : 4;
    const S32 gap = mCompact ? 4 : 8;
    const S32 width = llmax(1, (getRect().getWidth() - gap * (columns - 1)) / columns);
    const S32 left = visible_index * (width + gap);
    return LLRect(left, getRect().getHeight(), left + width, 0);
}

S32 ALVCamMonitorStrip::tileAt(S32 x, S32 y) const
{
    const U32 start = mCompact ? 0 : mLargePage * 4;
    const U32 end = llmin(mCount, start + (mCompact ? 8u : 4u));
    for (U32 slot = start; slot < end; ++slot)
        if (tileRect(static_cast<S32>(slot - start)).pointInRect(x, y))
            return static_cast<S32>(slot);
    return -1;
}

void ALVCamMonitorStrip::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam monitor tile");
    const U32 start = mCompact ? 0 : mLargePage * 4;
    const U32 end = llmin(mCount, start + (mCompact ? 8u : 4u));
    for (U32 slot = start; slot < end; ++slot)
    {
        const Tile& tile = mTiles[slot];
        if (!tile.mVisible) continue;
        LLRect rect = tileRect(static_cast<S32>(slot - start));
        gl_rect_2d(rect, mBorderColor, false);
        LLRect picture(rect.mLeft + 2, rect.mTop - 2, rect.mRight - 2,
                       rect.mBottom + (mCompact ? 42 : 58));
        const LLRectf& uv = tile.mPublication.mCellUV;
        if (tile.mPublication.mAtlas.mGeneration &&
            mPreviewService->bindAtlas(tile.mPublication.mAtlas))
        {
            gGL.color4f(1.f, 1.f, 1.f, 1.f);
            gGL.begin(LLRender::TRIANGLES);
            gGL.texCoord2f(uv.mLeft, uv.mTop); gGL.vertex2i(picture.mLeft, picture.mTop);
            gGL.texCoord2f(uv.mLeft, uv.mBottom); gGL.vertex2i(picture.mLeft, picture.mBottom);
            gGL.texCoord2f(uv.mRight, uv.mBottom); gGL.vertex2i(picture.mRight, picture.mBottom);
            gGL.texCoord2f(uv.mLeft, uv.mTop); gGL.vertex2i(picture.mLeft, picture.mTop);
            gGL.texCoord2f(uv.mRight, uv.mBottom); gGL.vertex2i(picture.mRight, picture.mBottom);
            gGL.texCoord2f(uv.mRight, uv.mTop); gGL.vertex2i(picture.mRight, picture.mTop);
            gGL.end();
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        }
        else
            gl_rect_2d(picture, LLColor4(0.08f, 0.09f, 0.11f, 1.f), true);
        drawText(mFont, tile.mBadge, picture.mLeft + 12, picture.mTop - 13,
                 LLColor4(0.56f, 0.85f, 0.66f, 1.f));
        LLColor4 status_dot(0.56f, 0.85f, 0.66f, 1.f);
        if (tile.mBadge.find("PAUSED") != std::string::npos ||
            tile.mPublication.mState == LLPrismPreviewService::ETileState::PAUSED ||
            tile.mPublication.mState == LLPrismPreviewService::ETileState::HELD)
            status_dot = mMutedColor;
        else if (tile.mPublication.mState == LLPrismPreviewService::ETileState::WARMING ||
                 tile.mPublication.mState == LLPrismPreviewService::ETileState::THROTTLED)
            status_dot = LLColor4(0.95f, 0.68f, 0.22f, 1.f);
        else if (tile.mPublication.mState == LLPrismPreviewService::ETileState::OFFLINE ||
                 tile.mPublication.mState == LLPrismPreviewService::ETileState::FAILED ||
                 tile.mPublication.mState == LLPrismPreviewService::ETileState::SHADER_UNAVAILABLE)
            status_dot = LLColor4(0.90f, 0.28f, 0.24f, 1.f);
        gGL.color4fv(status_dot.mV);
        gl_circle_2d(static_cast<F32>(picture.mLeft + 6), static_cast<F32>(picture.mTop - 9),
                     2.5f, 10, true);
        if (!mCompact) drawText(mFont, tile.mTimestamp, picture.mRight - 70,
                                picture.mBottom + 4, mTextColor);
        drawText(mFont, tile.mName, rect.mLeft + 4, picture.mBottom - 13, mTextColor);
        if (!mCompact)
        {
            drawText(mFont, tile.mShows, rect.mLeft + 4, picture.mBottom - 28, mMutedColor);
            drawText(mFont, tile.mStatus, rect.mLeft + 4, picture.mBottom - 43, mMutedColor);
        }
        drawText(mFont, "::::", rect.mLeft + 4, rect.mBottom + 4, mMutedColor);
        drawText(mFont, "...", rect.mRight - 18, picture.mBottom - 13, mMutedColor);
    }
    LLUICtrl::draw();
}

bool ALVCamMonitorStrip::handleMouseDown(S32 x, S32 y, MASK)
{
    const S32 slot = tileAt(x, y);
    if (slot < 0) return false;
    const LLRect rect = tileRect(mCompact ? slot : slot - static_cast<S32>(mLargePage * 4));
    if (x > rect.mRight - 28)
    {
        LLSD value;
        value["action"] = "menu";
        value["slot"] = slot;
        value["x"] = x;
        value["y"] = y;
        setValue(value);
        onCommit();
        return true;
    }
    mMouseDownSlot = slot;
    S32 screen_x = 0;
    S32 screen_y = 0;
    localPointToScreen(x, y, &screen_x, &screen_y);
    LLToolDragAndDrop::instance().setDragStart(screen_x, screen_y);
    gFocusMgr.setMouseCapture(this);
    return true;
}

bool ALVCamMonitorStrip::handleHover(S32 x, S32 y, MASK mask)
{
    S32 screen_x = 0;
    S32 screen_y = 0;
    localPointToScreen(x, y, &screen_x, &screen_y);
    if (mMouseDownSlot >= 0 && gFocusMgr.getMouseCapture() == this &&
        LLToolDragAndDrop::instance().isOverThreshold(screen_x, screen_y))
    {
        std::string reason;
        const S32 slot = mMouseDownSlot;
        mMouseDownSlot = -1;
        if (!beginDragFromTile(mTiles[slot].mCapture, mNewFaceFit, &reason))
        {
            setToolTip(LLStringExplicit(reason));
            LLSD value;
            value["action"] = "drag_rejected";
            value["reason"] = reason;
            setValue(value);
            onCommit();
            // Keep capture until mouse-up so the transient rejection tooltip
            // can be cleared at the actual end of the attempted drag.
            return true;
        }
        gFocusMgr.setMouseCapture(nullptr);
        return true;
    }
    return LLUICtrl::handleHover(x, y, mask);
}

bool ALVCamMonitorStrip::handleMouseUp(S32, S32, MASK)
{
    const bool handled = gFocusMgr.getMouseCapture() == this || mMouseDownSlot >= 0;
    mMouseDownSlot = -1;
    setToolTip(LLStringUtil::null);
    if (gFocusMgr.getMouseCapture() == this) gFocusMgr.setMouseCapture(nullptr);
    return handled;
}

bool ALVCamMonitorStrip::handleRightMouseDown(S32 x, S32 y, MASK)
{
    const S32 slot = tileAt(x, y);
    if (slot < 0) return false;
    LLSD value;
    value["action"] = "menu";
    value["slot"] = slot;
    value["x"] = x;
    value["y"] = y;
    setValue(value);
    onCommit();
    return true;
}

bool ALVCamMonitorStrip::beginDragFromTile(
    const LLPrismLens::CaptureHandle& capture,
    LLPrismLens::EFitMode new_face_fit, std::string* reason)
{
    LLPrismLens::CaptureHandle current;
    LLPrismLens::ECaptureMode mode = LLPrismLens::ECaptureMode::SURFACE_LENS;
    if (!currentCapture(capture.mId, current, &mode) || !sameCapture(current, capture))
    {
        if (reason) *reason = "That capture no longer exists.";
        return false;
    }
    if (mode == LLPrismLens::ECaptureMode::SURFACE_LENS)
    {
        if (reason) *reason = "A Surface Lens owns exactly one immutable aperture display.";
        return false;
    }
    if (new_face_fit != LLPrismLens::EFitMode::FIT &&
        new_face_fit != LLPrismLens::EFitMode::FILL &&
        new_face_fit != LLPrismLens::EFitMode::STRETCH)
    {
        if (reason) *reason = "Unknown display fit mode.";
        return false;
    }
    sActiveDragFit = new_face_fit;
    LLToolDragAndDrop::instance().beginDrag(
        DAD_VCAM_FEED, current.mId, LLToolDragAndDrop::SOURCE_VCAM);
    if (reason) reason->clear();
    return true;
}

bool ALVCamMonitorStrip::beginDragFromTile(
    const LLUUID& capture_id, LLPrismLens::EFitMode new_face_fit,
    std::string* reason)
{
    LLPrismLens::CaptureHandle capture;
    if (!currentCapture(capture_id, capture))
    {
        if (reason) *reason = "That capture no longer exists.";
        return false;
    }
    return beginDragFromTile(capture, new_face_fit, reason);
}

LLPrismLens::EFitMode ALVCamMonitorStrip::activeDragFitMode()
{
    return sActiveDragFit;
}

bool ALVCamMonitorStrip::handleDisplayRowDrop(
    const LLPrismLens::DisplayHandle& display, bool drop,
    EDragAndDropType cargo_type, void* cargo_data, EAcceptance* accept,
    std::string& tooltip_msg)
{
    if (cargo_type != DAD_VCAM_FEED || !cargo_data ||
        LLToolDragAndDrop::instance().getSource() != LLToolDragAndDrop::SOURCE_VCAM)
        return false;
    LLPrismLens::CaptureHandle capture;
    const LLUUID& capture_id = *static_cast<const LLUUID*>(cargo_data);
    if (!currentCapture(capture_id, capture))
    {
        *accept = ACCEPT_NO_CUSTOM;
        tooltip_msg = "That capture no longer exists.";
        return true;
    }
    const LLPrismLens::ActionStatus status =
        ALVCamPageModel::reassignDisplayStatus(display, capture);
    if (!status.allowed())
    {
        *accept = ACCEPT_NO_CUSTOM;
        tooltip_msg = status.mReason;
        return true;
    }
    *accept = ACCEPT_YES_SINGLE;
    tooltip_msg.clear();
    if (drop)
    {
        std::string reason;
        if (LLPrismLens::reassignDisplay(display, capture, &reason) !=
            LLPrismLens::ERegistryResult::OK)
        {
            *accept = ACCEPT_NO_CUSTOM;
            tooltip_msg = reason;
        }
    }
    return true;
}

ALVCamMonitorPager::ALVCamMonitorPager(const Params& p)
    : LLUICtrl(p), mFont(LLFontGL::getFontSansSerifSmall()),
      mTextColor(ALKitStyle::color(ALKitStyle::TEXT).get())
{
}

void ALVCamMonitorPager::setPage(U32 page, U32 count)
{
    mPage = llmin(page, 1u);
    mCount = count;
    mLabel = count > 4
        ? llformat(mPage ? "<  1-4 / 5-%u" : "1-4 / 5-%u  >", count)
        : "1-4";
}

void ALVCamMonitorPager::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam monitor pager");
    drawText(mFont, mLabel, 4, 7, mTextColor);
    LLUICtrl::draw();
}

bool ALVCamMonitorPager::handleMouseDown(S32 x, S32, MASK)
{
    if (mCount <= 4) return false;
    mPage = x < getRect().getWidth() / 2 ? 0 : 1;
    setValue(static_cast<S32>(mPage));
    onCommit();
    return true;
}

ALVCamCameraSelector::ALVCamCameraSelector(const Params& p)
    : LLUICtrl(p), mFont(LLFontGL::getFontSansSerifSmall()),
      mTextColor(ALKitStyle::color(ALKitStyle::TEXT).get()),
      mMutedColor(ALKitStyle::color(ALKitStyle::MUTED).get()),
      mAccentColor(ALKitStyle::color(ALKitStyle::ACCENT).get()),
      mBorderColor(ALKitStyle::color(ALKitStyle::CARD_BORDER).get())
{
}

bool ALVCamCameraSelector::postBuild()
{
    if (!LLUICtrl::postBuild())
        return false;

    LLComboBox::Params add_params;
    add_params.name = "vc_add_camera";
    add_params.rect = LLRect(0, 24, 112, 0);
    mAdd = LLUICtrlFactory::create<LLComboBox>(add_params);
    mAdd->add("+ Add camera...", LLSD());
    mAdd->add("From selected object", LLSD("object"));
    mAdd->add("Virtual at my view", LLSD("virtual"));
    mAdd->add("OTS pair", LLSD("ots"));
    mAdd->add("Surface lens", LLSD("lens"));
    addChild(mAdd);

    LLButton::Params remove_params;
    remove_params.name = "vc_remove_capture";
    remove_params.rect = LLRect(0, 24, 106, 0);
    remove_params.label = "Remove capture";
    mRemove = LLUICtrlFactory::create<LLButton>(remove_params);
    ALKitStyle::styleButton(mRemove, false);
    addChild(mRemove);
    reshape(getRect().getWidth(), getRect().getHeight(), true);
    return true;
}

void ALVCamCameraSelector::setItems(
    const std::vector<Item>& items, const LLPrismLens::CaptureHandle& selected)
{
    mItems = items;
    mSelected = selected;
}

void ALVCamCameraSelector::setActionsEnabled(bool can_add, bool can_remove)
{
    if (mAdd) mAdd->setEnabled(can_add);
    if (mRemove) mRemove->setEnabled(can_remove);
}

void ALVCamCameraSelector::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLUICtrl::reshape(width, height, called_from_parent);
    const S32 gap = 6;
    const S32 remove_width = llmin(106, width / 4);
    const S32 add_width = llmin(112, width / 4);
    mChipWidth = llmax(1, width - add_width - remove_width - gap * 2);
    if (mAdd)
    {
        mAdd->reshape(add_width, 24, true);
        mAdd->setOrigin(mChipWidth + gap, (height - 24) / 2);
    }
    if (mRemove)
    {
        mRemove->reshape(remove_width, 24, true);
        mRemove->setOrigin(mChipWidth + gap * 2 + add_width, (height - 24) / 2);
    }
}

void ALVCamCameraSelector::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam camera selector");
    if (mItems.empty())
        drawText(mFont, "Add a camera to start", 4, 8, mMutedColor);
    const S32 width = llmax(1, mChipWidth / llmax(1, static_cast<S32>(mItems.size())));
    for (S32 i = 0; i < static_cast<S32>(mItems.size()); ++i)
    {
        const bool selected = sameCapture(mItems[i].mHandle, mSelected);
        LLRect rect(i * width, getRect().getHeight(), (i + 1) * width - 3, 0);
        gl_rect_2d(rect, selected ? mAccentColor : mBorderColor, false);
        drawText(mFont, mItems[i].mLabel, rect.mLeft + 5, 8, mTextColor);
    }
    LLUICtrl::draw();
}

bool ALVCamCameraSelector::handleMouseDown(S32 x, S32, MASK)
{
    if (mItems.empty() || x >= mChipWidth) return false;
    const S32 index = llclamp(x * static_cast<S32>(mItems.size()) /
                              llmax(1, mChipWidth), 0,
                              static_cast<S32>(mItems.size()) - 1);
    setValue(LLSD(mItems[index].mHandle.mId));
    onCommit();
    return true;
}

ALVCamFaceList::ALVCamFaceList(const Params& p)
    : LLUICtrl(p), mFont(LLFontGL::getFontSansSerifSmall()),
      mTextColor(ALKitStyle::color(ALKitStyle::TEXT).get()),
      mMutedColor(ALKitStyle::color(ALKitStyle::MUTED).get()),
      mAccentColor(ALKitStyle::color(ALKitStyle::ACCENT).get()),
      mBorderColor(ALKitStyle::color(ALKitStyle::CARD_BORDER).get())
{
}

void ALVCamFaceList::setItems(const std::vector<Item>& items)
{
    mItems = items;
    setScrollOffset(mScrollOffset);
}

S32 ALVCamFaceList::rowAt(S32 y) const
{
    const S32 row = mScrollOffset + (getRect().getHeight() - 1 - y) / FACE_ROW_HEIGHT;
    return row >= 0 && row < static_cast<S32>(mItems.size()) ? row : -1;
}

void ALVCamFaceList::setScrollOffset(S32 offset)
{
    const S32 visible_rows = llmax(1, getRect().getHeight() / FACE_ROW_HEIGHT);
    mScrollOffset = llclamp(offset, 0, llmax(0, static_cast<S32>(mItems.size()) - visible_rows));
}

void ALVCamFaceList::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam face list");
    const S32 visible_rows = llmax(1, (getRect().getHeight() + FACE_ROW_HEIGHT - 1) / FACE_ROW_HEIGHT);
    const S32 end = llmin(static_cast<S32>(mItems.size()), mScrollOffset + visible_rows);
    const S32 content_right = getRect().getWidth() - FACE_SCROLLBAR_WIDTH - 2;
    for (S32 i = mScrollOffset; i < end; ++i)
    {
        const S32 top = getRect().getHeight() - (i - mScrollOffset) * FACE_ROW_HEIGHT;
        LLRect rect(0, top, content_right, top - 30);
        gl_rect_2d(rect, mItems[i].mSelected ? mAccentColor : mBorderColor, false);
        drawText(mFont, mItems[i].mName, 7, top - 13, mTextColor);
        drawText(mFont, mItems[i].mMeta, 150, top - 13, mMutedColor);
    }
    if (static_cast<S32>(mItems.size()) > visible_rows)
    {
        const S32 track_left = getRect().getWidth() - FACE_SCROLLBAR_WIDTH;
        const S32 thumb_height = llmax(18, getRect().getHeight() * visible_rows /
                                           static_cast<S32>(mItems.size()));
        const S32 max_offset = static_cast<S32>(mItems.size()) - visible_rows;
        const S32 travel = getRect().getHeight() - thumb_height;
        const S32 thumb_top = getRect().getHeight() -
            (max_offset ? mScrollOffset * travel / max_offset : 0);
        gl_rect_2d(LLRect(track_left, getRect().getHeight(), getRect().getWidth(), 0),
                   LLColor4(0.f, 0.f, 0.f, .18f), true);
        gl_rect_2d(LLRect(track_left + 1, thumb_top, getRect().getWidth() - 1,
                          thumb_top - thumb_height), mBorderColor, true);
    }
    if (mMovePick)
    {
        gl_rect_2d(getLocalRect(), mAccentColor, false);
        drawText(mFont, "PICK A FACE TO MOVE", llmax(7, content_right - 132),
                 getRect().getHeight() - 13, mAccentColor);
    }
    LLUICtrl::draw();
}

bool ALVCamFaceList::handleMouseDown(S32 x, S32 y, MASK)
{
    if (x >= getRect().getWidth() - FACE_SCROLLBAR_WIDTH)
    {
        const S32 visible_rows = llmax(1, getRect().getHeight() / FACE_ROW_HEIGHT);
        const S32 max_offset = llmax(0, static_cast<S32>(mItems.size()) - visible_rows);
        setScrollOffset((getRect().getHeight() - y) * max_offset /
                        llmax(1, getRect().getHeight()));
        return true;
    }
    const S32 row = rowAt(y);
    if (row < 0) return false;
    setValue(mItems[row].mRaw ? LLSD() : LLSD(mItems[row].mHandle.mId));
    onCommit();
    return true;
}

bool ALVCamFaceList::handleScrollWheel(S32, S32, LLScrollDelta delta)
{
    setScrollOffset(mScrollOffset + delta.mClicks);
    return true;
}

bool ALVCamFaceList::handleDragAndDrop(
    S32, S32 y, MASK, bool drop, EDragAndDropType cargo_type,
    void* cargo_data, EAcceptance* accept, std::string& tooltip_msg)
{
    const S32 row = rowAt(y);
    if (row < 0 || mItems[row].mRaw) return false;
    return ALVCamMonitorStrip::handleDisplayRowDrop(mItems[row].mHandle, drop, cargo_type,
                                cargo_data, accept, tooltip_msg);
}
