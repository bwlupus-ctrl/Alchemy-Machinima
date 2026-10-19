/**
 * @file alvcammonitorstrip.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "alvcammonitorstrip.h"

#include "alkitstyle.h"
#include "alkitcard.h"
#include "lluicolortable.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llfontgl.h"
#include "llrender.h"
#include "llrender2dutils.h"
#include "llsliderwheel.h"
#include "lltextbox.h"
#include "lltimer.h"
#include "lltooldraganddrop.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

#include <algorithm>

static LLDefaultChildRegistry::Register<ALVCamMonitorStrip>
    r_vcam_monitor_strip("vcam_monitor_strip");
static LLDefaultChildRegistry::Register<ALVCamMonitorHeader>
    r_vcam_monitor_header("vcam_monitor_header");
static LLDefaultChildRegistry::Register<ALVCamMonitorTileInfo>
    r_vcam_monitor_tile("vcam_monitor_tile");
static LLDefaultChildRegistry::Register<ALVCamMonitorPager>
    r_vcam_monitor_pager("vcam_monitor_pager");
static LLDefaultChildRegistry::Register<ALVCamCameraSelector>
    r_vcam_camera_selector("vcam_camera_selector");
static LLDefaultChildRegistry::Register<ALVCamFaceList>
    r_vcam_face_list("vcam_face_list");
static LLDefaultChildRegistry::Register<ALVCamTexturePickerList>
    r_vcam_texture_picker_list("vcam_texture_picker_list");
static LLDefaultChildRegistry::Register<ALVCamProjectorList>
    r_vcam_projector_list("vcam_projector_list");

namespace
{
LLPrismLens::EFitMode sActiveDragFit = LLPrismLens::EFitMode::FIT;
constexpr S32 FACE_ROW_HEIGHT = 32;
constexpr S32 FACE_SCROLLBAR_WIDTH = 10;
constexpr S32 PICKER_ROW_HEIGHT = 35;
constexpr S32 PROJECTOR_ROW_HEIGHT = 36;
constexpr S32 PROJECTOR_SCROLLBAR_WIDTH = 10;

struct MonitorHeaderLayout
{
    S32 mSizeLabelLeft;
    S32 mSizeLeft;
    S32 mSizeRight;
    S32 mPreviewLeft;
    S32 mPreviewRight;
    S32 mInfoLeft;
    S32 mInfoRight;
    S32 mSpeedLabelLeft;
    S32 mSpeedLeft;
};

MonitorHeaderLayout monitorHeaderLayout(S32)
{
    // Two rows fit the minimum kit card. Drawing and hit testing use the same
    // tracks; no right-aligned group can push a switch outside the card.
    return {0, 30, 160, 172, 272, 0, 72, 88, 120};
}

S32 displayTenths(F32 value)
{
    return ll_round(value * 10.f);
}

bool sameCapture(const LLPrismLens::CaptureHandle& left,
                 const LLPrismLens::CaptureHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

bool currentCapture(const LLUUID& id, LLPrismLens::CaptureHandle& capture,
                    bool* surface = nullptr)
{
    const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mSlotCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate = snapshot.mCaptures[index];
        if (candidate.mHandle.mId == id)
        {
            capture = candidate.mHandle;
            if (surface) *surface = candidate.isSurfaceLens();
            return true;
        }
    }
    return false;
}

void drawText(const LLFontGL* font, const std::string& text,
              S32 x, S32 y, const LLColor4& color, S32 max_pixels = S32_MAX)
{
    if (font) font->renderUTF8(text, 0, static_cast<F32>(x), static_cast<F32>(y), color, LLFontGL::LEFT,
                              LLFontGL::BASELINE, LLFontGL::NORMAL, LLFontGL::DROP_SHADOW_SOFT,
                              S32_MAX, llmax(1, max_pixels), nullptr, max_pixels != S32_MAX);
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

void ALVCamMonitorHeader::setState(bool compact, bool previews, bool info_overlay, F32 speed)
{
    mCompact = compact;
    mPreviews = previews;
    mInfoOverlay = info_overlay;
    mSpeed = speed;
}

void ALVCamMonitorHeader::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    const S32 w = getRect().getWidth();
    const MonitorHeaderLayout layout = monitorHeaderLayout(w);
    const S32 h = 24;
    gGL.pushMatrix();
    gGL.translatef(0.f, static_cast<F32>(getRect().getHeight() - h), 0.f);
    drawText(mFont, "Size", layout.mSizeLabelLeft, 8, mMutedColor);
    const S32 size_width = llmax(1, layout.mSizeRight - layout.mSizeLeft);
    for (S32 index = 0; index < 2; ++index)
    {
        LLRect rect(layout.mSizeLeft + index * size_width / 2, h,
                    layout.mSizeLeft + (index + 1) * size_width / 2 - 2, 0);
        gl_rect_2d(rect, index == static_cast<S32>(mCompact) ? mAccentColor : mBorderColor, false);
        drawText(mFont, index ? "Compact" : "Large", rect.mLeft + 5, 8, mTextColor,
                 rect.getWidth() - 10);
    }
    const S32 preview_switch_left = layout.mPreviewRight - ALKitStyle::SWITCH_W;
    drawText(mFont, "Previews", layout.mPreviewLeft, 8, mTextColor,
             preview_switch_left - layout.mPreviewLeft - 4);
    const LLColor4 preview_track = (mPreviews ? mAccentColor :
        ALKitStyle::color(ALKitStyle::SWITCH_OFF).get());
    const LLColor4 preview_knob = ALKitStyle::color(mPreviews ? ALKitStyle::KNOB_ON :
        ALKitStyle::KNOB_OFF).get();
    gGL.color4fv(preview_track.mV);
    gl_rect_2d(preview_switch_left + 9, 21, preview_switch_left + 25, 3,
               preview_track, true);
    gl_circle_2d(static_cast<F32>(preview_switch_left + 9), 12.f, 9.f, 20, true);
    gl_circle_2d(static_cast<F32>(preview_switch_left + 25), 12.f, 9.f, 20, true);
    gGL.color4fv(preview_knob.mV);
    gl_circle_2d(static_cast<F32>(preview_switch_left + (mPreviews ? 25 : 9)),
                 12.f, 7.f, 20, true);
    gGL.popMatrix();
    const S32 info_switch_left = layout.mInfoRight - ALKitStyle::SWITCH_W;
    drawText(mFont, "Info", layout.mInfoLeft, 8, mTextColor,
             info_switch_left - layout.mInfoLeft - 4);
    const LLColor4 info_track = (mInfoOverlay ? mAccentColor :
        ALKitStyle::color(ALKitStyle::SWITCH_OFF).get());
    const LLColor4 info_knob = ALKitStyle::color(mInfoOverlay ? ALKitStyle::KNOB_ON :
        ALKitStyle::KNOB_OFF).get();
    gGL.color4fv(info_track.mV);
    gl_rect_2d(info_switch_left + 9, 21, info_switch_left + 25, 3, info_track, true);
    gl_circle_2d(static_cast<F32>(info_switch_left + 9), 12.f, 9.f, 20, true);
    gl_circle_2d(static_cast<F32>(info_switch_left + 25), 12.f, 9.f, 20, true);
    gGL.color4fv(info_knob.mV);
    gl_circle_2d(static_cast<F32>(info_switch_left + (mInfoOverlay ? 25 : 9)),
                 12.f, 7.f, 20, true);
    drawText(mFont, "fps", layout.mSpeedLabelLeft, 8, mMutedColor);
    const F32 speeds[] = { .5f, 1.f, 2.f, 5.f };
    const char* labels[] = { "0.5", "1", "2", "5" };
    const S32 speed_width = llmin(196, llmax(1, w - layout.mSpeedLeft));
    for (S32 index = 0; index < 4; ++index)
    {
        LLRect rect(layout.mSpeedLeft + index * speed_width / 4, h,
                    layout.mSpeedLeft + (index + 1) * speed_width / 4 - 2, 0);
        gl_rect_2d(rect, fabsf(mSpeed - speeds[index]) < .01f ? mAccentColor : mBorderColor, false);
        drawText(mFont, labels[index], rect.mLeft + 5, 8, mTextColor,
                 rect.getWidth() - 10);
    }
    LLUICtrl::draw();
}

bool ALVCamMonitorHeader::handleMouseDown(S32 x, S32 y, MASK)
{
    const S32 w = getRect().getWidth();
    const MonitorHeaderLayout layout = monitorHeaderLayout(w);
    LLSD action;
    const bool upper = y >= getRect().getHeight() - 24;
    const bool lower = y >= 0 && y < 24;
    if (upper && x >= layout.mSizeLeft && x < layout.mSizeRight)
    {
        action["action"] = "size";
        action["value"] = x >= layout.mSizeLeft +
            llmax(1, layout.mSizeRight - layout.mSizeLeft) / 2;
    }
    else if (upper && x >= layout.mPreviewLeft && x < layout.mPreviewRight)
    {
        action["action"] = "previews";
        action["value"] = !mPreviews;
    }
    else if (lower && x >= layout.mInfoLeft && x < layout.mInfoRight)
    {
        action["action"] = "info";
        action["value"] = !mInfoOverlay;
    }
    else
    {
        const F32 speeds[] = { .5f, 1.f, 2.f, 5.f };
        const S32 speed_width = llmin(196, llmax(1, w - layout.mSpeedLeft));
        if (!lower || x < layout.mSpeedLeft || x >= layout.mSpeedLeft + speed_width) return false;
        const S32 index = llclamp((x - layout.mSpeedLeft) * 4 /
                                      speed_width, 0, 3);
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
    U32 count, bool compact, bool info_overlay, U32 large_page)
{
    const bool force_info = info_overlay && !mInfoOverlay;
    for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
        if (tiles[slot].mVisible)
            updateTileText(slot, tiles[slot], info_overlay, force_info);
    mTiles = tiles;
    mCount = llmin(count, LLPrismLens::MAX_CAPTURES);
    mCompact = compact;
    mInfoOverlay = info_overlay;
    mLargePage = llmin(large_page, 1u);
}

void ALVCamMonitorStrip::updateTileText(
    U32 slot, const Tile& tile, bool, bool)
{
    TileTextCache& cache = mTileText[slot];
    if (!cache.mValid || cache.mLabelInput != tile.mLabel || cache.mMedia != tile.mMedia || cache.mGate != tile.mGate)
        cache.mName = llformat(tile.mGate ? "GATE %u - %s" : tile.mMedia ? "MEDIA %u - %s" : "CAM %u - %s", slot + 1, tile.mLabel.c_str());
    // State/reason only. Frame times, rates, and publication serials never
    // participate in text invalidation. The atlas refresh remains independent.
    if (cache.mStatus != tile.mStatus) cache.mStatus = tile.mStatus;
    cache.mLabelInput = tile.mLabel;
    cache.mMedia = tile.mMedia;
    cache.mGate = tile.mGate;
    cache.mValid = true;
}

const std::string& ALVCamMonitorStrip::previewCostText(bool previews_enabled, F32 preview_fps)
{
    const S32 preview_tenths = displayTenths(preview_fps);
    if (mPreviewCostText.empty() || mPreviewCostEnabled != previews_enabled ||
        mPreviewCostTenths != preview_tenths)
    {
        mPreviewCostText = previews_enabled
            ? llformat("On - %.1f fps", preview_tenths / 10.f)
            : "Off (no cost)";
        mPreviewCostEnabled = previews_enabled;
        mPreviewCostTenths = preview_tenths;
    }
    return mPreviewCostText;
}

const std::string& ALVCamMonitorStrip::monitorFooterText(
    bool master_enabled, bool previews_enabled, U32 monitor_count,
    U32 preview_only, F32 preview_fps)
{
    const S32 preview_tenths = displayTenths(preview_fps);
    if (mMonitorFooterText.empty() || mMonitorFooterMasterEnabled != master_enabled ||
        mMonitorFooterPreviewsEnabled != previews_enabled ||
        mMonitorFooterCount != monitor_count ||
        mMonitorFooterPreviewOnly != preview_only ||
        mMonitorFooterTenths != preview_tenths)
    {
        if (!master_enabled)
            mMonitorFooterText = "Virtual Cam is off, so monitors and faces are paused.";
        else if (!previews_enabled)
            mMonitorFooterText = llformat(
                "Previews are off: 0 of %u monitors render. Faces in-world keep updating at their own picture rate.",
                monitor_count);
        else if (!preview_only)
            mMonitorFooterText = llformat(
                "%u monitors at %.1f fps, all reusing their camera's last picture: no extra renders. Drag a monitor onto a surface to show it there.",
                monitor_count, preview_tenths / 10.f);
        else
            mMonitorFooterText = llformat(
                "%u monitors at %.1f fps. %u preview-only camera%s use the shared capture budget. Drag a monitor onto a surface to show it there.",
                monitor_count, preview_tenths / 10.f, preview_only,
                preview_only == 1 ? "" : "s");
        mMonitorFooterMasterEnabled = master_enabled;
        mMonitorFooterPreviewsEnabled = previews_enabled;
        mMonitorFooterCount = monitor_count;
        mMonitorFooterPreviewOnly = preview_only;
        mMonitorFooterTenths = preview_tenths;
    }
    return mMonitorFooterText;
}

LLRect ALVCamMonitorStrip::tileRect(S32 visible_index) const
{
    const S32 columns = mCompact ? 8 : 4;
    const S32 gap = mCompact ? 4 : 8;
    const S32 available = llmax(columns, getRect().getWidth() - gap * (columns - 1));
    const S32 left = visible_index * gap + available * visible_index / columns;
    const S32 right = visible_index * gap + available * (visible_index + 1) / columns;
    return LLRect(left, getRect().getHeight(), llmin(getRect().getWidth(), right), 0);
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
    if (start >= end)
    {
        drawText(mFont, "Add a camera to see monitor previews.", 4,
                 getRect().getHeight() / 2, mMutedColor, getRect().getWidth() - 8);
        LLUICtrl::draw();
        return;
    }
    for (U32 slot = start; slot < end; ++slot)
    {
        const Tile& tile = mTiles[slot];
        const TileTextCache& text_cache = mTileText[slot];
        if (!tile.mVisible) continue;
        const LLRect rect = tileRect(static_cast<S32>(slot - start));
        const S32 picture_width = llmax(1, rect.getWidth() - 4);
        const S32 caption_height = 42;
        const S32 picture_height = llmin(llmax(1, rect.getHeight() - caption_height),
                                         llmax(1, picture_width * 9 / 16));
        const LLRect picture(rect.mLeft + 2, rect.mTop - 2, rect.mRight - 2,
                             rect.mTop - 2 - picture_height);
        const LLRectf& uv = tile.mPublication.mCellUV;
        if (!tile.mNoSignal && tile.mPublication.mAtlas.mGeneration &&
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
            gl_rect_2d(picture, tile.mNoSignalWhite ? LLColor4::white : LLColor4::black, true);
        gl_rect_2d(picture, mBorderColor, false);
        LLColor4 status_dot(0.56f, 0.85f, 0.66f, 1.f);
        if (!tile.mPreviewsEnabled && tile.mActivity != LLPrismLens::EActivityState::IDLE)
            status_dot = mMutedColor;
        // HELD = reusing the camera's last picture between preview refreshes
        // (e.g. 1 fps). That is still a running camera: keep the steady live
        // colour, otherwise the dot flashes once per preview publish.
        else if (tile.mPublication.mState == LLPrismPreviewService::ETileState::PAUSED)
            status_dot = mMutedColor;
        else if (tile.mPublication.mState == LLPrismPreviewService::ETileState::WARMING ||
                 tile.mPublication.mState == LLPrismPreviewService::ETileState::THROTTLED)
            status_dot = LLColor4(0.95f, 0.68f, 0.22f, 1.f);
        else if (tile.mPublication.mState == LLPrismPreviewService::ETileState::OFFLINE ||
                 tile.mPublication.mState == LLPrismPreviewService::ETileState::FAILED ||
                 tile.mPublication.mState == LLPrismPreviewService::ETileState::SHADER_UNAVAILABLE)
            status_dot = LLColor4(0.90f, 0.28f, 0.24f, 1.f);
        if (mCompact)
        {
            gGL.color4fv(status_dot.mV);
            gl_circle_2d(static_cast<F32>(picture.mLeft + 6),
                         static_cast<F32>(picture.mTop - 7), 2.5f, 10, true);
        }
        const S32 name_y = picture.mBottom - 13;
        gGL.color4fv(status_dot.mV);
        if (!mCompact)
            gl_circle_2d(static_cast<F32>(rect.mLeft + 24), static_cast<F32>(name_y + 3),
                         2.5f, 10, true);
        drawText(mFont, "::::", rect.mLeft + 4, name_y, mMutedColor, 18);
        const S32 name_left = rect.mLeft + (mCompact ? 24 : 30);
        drawText(mFont, text_cache.mName, name_left, name_y, mTextColor,
                 llmax(1, rect.mRight - 22 - name_left));
        drawText(mFont, "...", rect.mRight - 18, name_y, mMutedColor, 16);
        {
            drawText(mFont, text_cache.mStatus, rect.mLeft + 12, picture.mBottom - 28,
                     status_dot, rect.getWidth() - 16);
        }
    }
    LLUICtrl::draw();
}

bool ALVCamMonitorStrip::handleMouseDown(S32 x, S32 y, MASK)
{
    const S32 slot = tileAt(x, y);
    if (slot < 0) return false;
    const LLRect rect = tileRect(mCompact ? slot : slot - static_cast<S32>(mLargePage * 4));
    const S32 picture_width = llmax(1, rect.getWidth() - 4);
    const S32 caption_height = 42;
    const S32 picture_height = llmin(llmax(1, rect.getHeight() - caption_height),
                                     llmax(1, picture_width * 9 / 16));
    const S32 name_y = rect.mTop - 2 - picture_height - 13;
    const LLRect menu_rect(rect.mRight - 26, name_y + 17,
                           rect.mRight, name_y - 5);
    if (menu_rect.pointInRect(x, y))
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
        // beginDrag() transfers capture from this strip to the drag tool.
        // Clearing global capture here would immediately call the tool's
        // onMouseCaptureLost() and erase its VCAM cargo.
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
    bool surface = false;
    if (!currentCapture(capture.mId, current, &surface) || !sameCapture(current, capture))
    {
        if (reason) *reason = "That capture no longer exists.";
        return false;
    }
    if (surface)
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
    const std::string second = count == 5 ? "5" : llformat("5-%u", count);
    mLabel = count > 4
        ? (mPage ? "<  1-4 / " + second : "1-4 / " + second + "  >")
        : llformat("1-%u", llmax(1u, count));
}

void ALVCamMonitorPager::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam monitor pager");
    drawText(mFont, mLabel, 4, 7, mTextColor, getRect().getWidth() - 8);
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
    : LLUICtrl(p), mFont(ALKitStyle::sansFont("Kit12")),
      mLiveColor(LLUIColorTable::instance().getColor("KitOkGreen"))
{
    mChipImage = LLUI::getUIImage("Kit_RoundRect4");
    mSelectedFont = LLFontGL::getFont(LLFontDescriptor("KitSansSemibold", "Kit12", 0));
    if (!mSelectedFont) mSelectedFont = ALKitStyle::sansFont("Kit12", true);
}

bool ALVCamCameraSelector::postBuild()
{
    if (!LLUICtrl::postBuild()) return false;
    LLTextBox::Params label_params;
    label_params.name = "vc_settings_for_label";
    label_params.rect = LLRect(0, 24, mFont->getWidth("Settings for"), 0);
    label_params.initial_value = "Settings for";
    label_params.font = mFont;
    label_params.text_color = ALKitStyle::color(ALKitStyle::DIM);
    label_params.v_pad = 0;
    label_params.font_valign = LLFontGL::VCENTER;
    mLabel = LLUICtrlFactory::create<LLTextBox>(label_params);
    addChild(mLabel);
    LLComboBox::Params add_params;
    add_params.name = "vc_add_camera";
    add_params.rect = LLRect(0, 24, 156, 0);
    mAdd = LLUICtrlFactory::create<LLComboBox>(add_params);
    mAdd->add("+ Add camera\xE2\x80\xA6", LLSD());
    mAdd->setToolTip(std::string("Add a camera"));
    mAdd->add("From selected object", LLSD("object"));
    mAdd->add("Virtual cam at my view", LLSD("virtual"));
    mAdd->add("OTS pair from Subjects A/B", LLSD("ots"));
    mAdd->add("Surface lens from selected face", LLSD("lens"));
    mAdd->add("Media from a prim face (video)", LLSD("media"));
    // Index 6 matches vcam_page.json selector_actions; the panel's "gate"
    // action adds the scene's single Gate slot (a second add reports why).
    mAdd->add("Gate (switch cameras and videos)", LLSD("gate"));
    mAdd->setEnabledByValue(LLSD("media"), false);
    ALKitStyle::styleCombo(mAdd);
    addChild(mAdd);

    LLButton::Params remove_params;
    remove_params.name = "vc_remove_capture";
    remove_params.rect = LLRect(0, 24, 110, 0);
    remove_params.label = "Remove capture";
    remove_params.pad_bottom = 0;
    mRemove = LLUICtrlFactory::create<LLButton>(remove_params);
    mRemove->setToolTip(std::string("Remove the selected capture"));
    ALKitStyle::styleButton(mRemove, true);
    mRemove->setVisible(false);
    addChild(mRemove);
    reshape(getRect().getWidth(), getRect().getHeight(), true);
    return true;
}

void ALVCamCameraSelector::requestLayout()
{
    reshape(getRect().getWidth(), getRect().getHeight(), true);
    for (LLView* parent = getParent(); parent; parent = parent->getParent())
        if (auto* card = dynamic_cast<ALKitCard*>(parent))
        {
            card->requestLayout();
            break;
        }
}

void ALVCamCameraSelector::selectChip(S32 index)
{
    if (index < 0 || index >= static_cast<S32>(mItems.size())) return;
    mSelected = mItems[index].mHandle;
    for (S32 i = 0; i < static_cast<S32>(mChips.size()); ++i)
    {
        mChips[i]->setToggleState(i == index);
        mChips[i]->setTabStop(i == index);
        mChips[i]->setFont(i == index ? mSelectedFont : mFont);
    }
    setValue(LLSD(mSelected.mId));
    onCommit();
}

void ALVCamCameraSelector::setItems(
    const std::vector<Item>& items, const LLPrismLens::CaptureHandle& selected)
{
    bool changed = items.size() != mItems.size();
    if (!changed)
        for (U32 i = 0; i < items.size(); ++i)
            changed = changed || items[i].mSlot != mItems[i].mSlot || items[i].mGate != mItems[i].mGate;
    mItems = items;
    mSelected = selected;
    while (mChips.size() > items.size())
    {
        LLButton* chip = mChips.back();
        removeChild(chip);
        delete chip;
        mChips.pop_back();
    }
    while (mChips.size() < items.size())
    {
        const S32 index = static_cast<S32>(mChips.size());
        LLButton::Params params;
        params.name = llformat("vc_camera_chip_%d", index);
        params.rect = LLRect(0, 24, 64, 0);
        params.is_toggle = true;
        params.pad_bottom = 0;
        LLButton* chip = LLUICtrlFactory::create<LLButton>(params);
        ALKitStyle::styleButton(chip, true);
        chip->setLeftHPad(22); // 10px padding + 6px dot + 6px gap.
        chip->setRightHPad(10);
        chip->setCommitCallback([this, index](LLUICtrl*, const LLSD&) { selectChip(index); });
        addChild(chip);
        mChips.push_back(chip);
    }
    for (U32 i = 0; i < items.size(); ++i)
    {
        const bool on = sameCapture(items[i].mHandle, selected);
        const std::string label = items[i].mGate ? "Gate" : llformat("Cam %u", items[i].mSlot + 1);
        const std::string help = items[i].mGate ? "Edit Gate" : llformat("Edit Cam %u \xC2\xB7 %s", items[i].mSlot + 1, items[i].mName.c_str());
        if (mChips[i]->getLabelUnselected() != label) mChips[i]->setLabel(label);
        if (mChips[i]->getToolTip() != help) mChips[i]->setToolTip(help);
        mChips[i]->setToggleState(on);
        mChips[i]->setTabStop(on || (selected.mId.isNull() && i == 0));
        const LLFontGL* font = on ? mSelectedFont : mFont;
        if (mChips[i]->getFont() != font) mChips[i]->setFont(font);
    }
    if (changed) requestLayout();
}

void ALVCamCameraSelector::setActionsEnabled(bool can_add, bool can_remove, bool media_enabled)
{
    if (mAdd && mMediaOptions != media_enabled)
    {
        mMediaOptions = media_enabled;
        // Keep the mockup's five options and the manifest's stable media
        // index, while retaining the existing feature gate and model guard.
        mAdd->setEnabledByValue(LLSD("media"), media_enabled);
        mAdd->selectFirstItem();
    }
    if (mAdd)
    {
        mAdd->setEnabled(can_add && mItems.size() < LLPrismLens::MAX_CAPTURES);
        // The mockup's visible addLabel differs from its accessibility label.
        mAdd->setLabel(LLStringExplicit(mItems.size() >= LLPrismLens::MAX_CAPTURES ? "8 of 8" : "+ Add camera\xE2\x80\xA6"));
    }
    if (mRemove)
    {
        const bool changed = mRemove->getVisible() != can_remove;
        mRemove->setEnabled(can_remove);
        mRemove->setVisible(can_remove);
        if (changed) requestLayout();
    }
}

ALVCamCameraSelector::Layout ALVCamCameraSelector::layoutForWidth(S32 width) const
{
    // CSS flex-wrap: outer gap 8px, radio group gap 4px. All coordinates are
    // measured downwards here, then translated once to the control's origin.
    Layout layout;
    width = llmax(1, width);
    S32 x = 0, y = 0, line_height = 24;
    auto place = [&](S32 w, S32 h) -> LLRect
    {
        w = llmin(width, w);
        if (x && x + w > width) { x = 0; y += line_height + 8; line_height = 24; }
        const LLRect rect(x, -y, x + w, -y - h);
        x += w + 8;
        line_height = llmax(line_height, h);
        return rect;
    };
    layout.mLabel = place(mFont->getWidth("Settings for"), 24);
    if (!mItems.empty())
    {
        std::vector<S32> widths;
        S32 total = 0;
        for (const Item& item : mItems)
        {
            const std::string label = item.mGate ? "Gate" : llformat("Cam %u", item.mSlot + 1);
            const S32 chip_width = llmax(mFont->getWidth(label), mSelectedFont->getWidth(label)) + 32;
            widths.push_back(chip_width);
            total += chip_width + 4;
        }
        const S32 group_width = llmin(width, total - 4);
        S32 cx = 0, cy = 0;
        for (S32 chip_width : widths)
        {
            chip_width = llmin(group_width, chip_width);
            if (cx && cx + chip_width > group_width) { cx = 0; cy += 28; }
            layout.mChips.emplace_back(cx, -cy, cx + chip_width, -cy - 24);
            cx += chip_width + 4;
        }
        const LLRect group = place(group_width, cy + 24);
        for (LLRect& chip : layout.mChips) chip.translate(group.mLeft, group.mTop);
    }
    layout.mAdd = place(156, 24);
    if (mRemove && mRemove->getVisible())
        layout.mRemove = place(mFont->getWidth("Remove capture") + 20, 24);
    layout.mHeight = y + line_height;
    return layout;
}

S32 ALVCamCameraSelector::heightForWidth(S32 width) const
{
    return layoutForWidth(width).mHeight;
}

void ALVCamCameraSelector::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLUICtrl::reshape(width, height, called_from_parent);
    mLayout = layoutForWidth(width);
    auto place = [height](LLView* view, LLRect rect)
    {
        if (!view) return;
        rect.translate(0, height);
        view->reshape(rect.getWidth(), rect.getHeight(), true);
        view->setOrigin(rect.mLeft, rect.mBottom);
    };
    place(mAdd, mLayout.mAdd);
    place(mLabel, mLayout.mLabel);
    if (mRemove && mRemove->getVisible()) place(mRemove, mLayout.mRemove);
    for (U32 i = 0; i < mChips.size(); ++i) place(mChips[i], mLayout.mChips[i]);
}

void ALVCamCameraSelector::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam camera selector");
    // Match the kit segments. Read live theme references, never cached RGBs.
    const LLColor4 border = ALKitStyle::color(ALKitStyle::INPUT_BORDER).get();
    auto rounded = [this](const LLRect& rect, const LLColor4& color)
    {
        if (mChipImage) mChipImage->draw(rect.mLeft, rect.mBottom, rect.getWidth(), rect.getHeight(), color);
    };
    for (U32 i = 0; i < mChips.size(); ++i)
    {
        const bool on = sameCapture(mItems[i].mHandle, mSelected);
        const LLRect rect = mChips[i]->getRect();
        rounded(rect, ALKitStyle::color(on ? ALKitStyle::ACCENT : ALKitStyle::INPUT_BORDER).get());
        LLRect inside = rect;
        inside.stretch(-1);
        rounded(inside, ALKitStyle::color(on ? ALKitStyle::ACTIVE_FILL : ALKitStyle::INPUT_BG).get());
    }
    if (mRemove && mRemove->getVisible())
    {
        // A 4px rounded outline with an untouched, transparent interior.
        const LLRect rect = mRemove->getRect();
        gl_line_2d(rect.mLeft + 4, rect.mTop, rect.mRight - 4, rect.mTop, border);
        gl_line_2d(rect.mLeft + 4, rect.mBottom, rect.mRight - 4, rect.mBottom, border);
        gl_line_2d(rect.mLeft, rect.mBottom + 4, rect.mLeft, rect.mTop - 4, border);
        gl_line_2d(rect.mRight, rect.mBottom + 4, rect.mRight, rect.mTop - 4, border);
        gGL.color4fv(border.mV);
        gl_arc_2d(rect.mRight - 4.f, rect.mTop - 4.f, 4.f, 8, false, 0.f, F_PI_BY_TWO);
        gl_arc_2d(rect.mLeft + 4.f, rect.mTop - 4.f, 4.f, 8, false, F_PI_BY_TWO, F_PI);
        gl_arc_2d(rect.mLeft + 4.f, rect.mBottom + 4.f, 4.f, 8, false, F_PI, F_PI + F_PI_BY_TWO);
        gl_arc_2d(rect.mRight - 4.f, rect.mBottom + 4.f, 4.f, 8, false, F_PI + F_PI_BY_TWO, F_TWO_PI);
    }
    LLUICtrl::draw();
    for (U32 i = 0; i < mChips.size(); ++i)
    {
        const LLRect rect = mChips[i]->getRect();
        const LLColor4 dot = mItems[i].mLive ? mLiveColor.get() :
            ALKitStyle::color(mItems[i].mPreview ? ALKitStyle::ACCENT : ALKitStyle::FAINT).get();
        gGL.color4fv(dot.mV);
        gl_circle_2d(static_cast<F32>(rect.mLeft + 13), static_cast<F32>(rect.getCenterY()), 3.f, 16, true);
    }
}

bool ALVCamCameraSelector::handleKeyHere(KEY key, MASK mask)
{
    if (mask == MASK_NONE && !mItems.empty() &&
        (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN || key == KEY_HOME || key == KEY_END))
    {
        S32 index = -1;
        for (U32 i = 0; i < mChips.size(); ++i)
            if (mChips[i]->hasFocus()) index = static_cast<S32>(i);
        if (index < 0) return LLUICtrl::handleKeyHere(key, mask);
        const S32 count = static_cast<S32>(mItems.size());
        index = key == KEY_HOME ? 0 : key == KEY_END ? count - 1 :
            (index + (key == KEY_LEFT || key == KEY_UP ? count - 1 : 1)) % count;
        mChips[index]->setFocus(true);
        selectChip(index);
        return true;
    }
    return LLUICtrl::handleKeyHere(key, mask);
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
    // Only consume the wheel when the list actually scrolled; otherwise let the page scroll.
    const S32 before = mScrollOffset;
    setScrollOffset(mScrollOffset + delta.mClicks);
    return mScrollOffset != before;
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

ALVCamTexturePickerList::ALVCamTexturePickerList(const Params& p)
    : LLUICtrl(p),
      mFont(LLFontGL::getFontSansSerifSmall()),
      mBorderColor(ALKitStyle::color(ALKitStyle::CARD_BORDER).get()),
      mTextColor(ALKitStyle::color(ALKitStyle::TEXT).get()),
      mMutedColor(ALKitStyle::color(ALKitStyle::MUTED).get()),
      mAccentColor(ALKitStyle::color(ALKitStyle::ACCENT).get()),
      mPreviewService(&LLPrismPreviewService::instance())
{
    setMouseOpaque(true);
}

void ALVCamTexturePickerList::setItems(
    const std::array<Item, LLPrismLens::MAX_CAPTURES>& items,
    U32 count, const LLUUID& selected)
{
    mItems = items;
    mCount = llmin(count, LLPrismLens::MAX_CAPTURES);
    mSelected = selected;
}

S32 ALVCamTexturePickerList::rowAt(S32 y) const
{
    const S32 row = (getRect().getHeight() - y) / PICKER_ROW_HEIGHT;
    return row >= 0 && row <= static_cast<S32>(mCount) ? row : -1;
}

void ALVCamTexturePickerList::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (!mCount)
    {
        drawText(mFont, "No VCam cameras - add one in Director > Camera > VCam",
                 8, getRect().getHeight() / 2 + 4, mMutedColor,
                 getRect().getWidth() - 16);
        LLUICtrl::draw();
        return;
    }
    for (U32 row = 0; row <= mCount; ++row)
    {
        const S32 top = getRect().getHeight() - static_cast<S32>(row) * PICKER_ROW_HEIGHT;
        const LLRect rect(0, top, getRect().getWidth(), top - PICKER_ROW_HEIGHT);
        const bool remove = row == 0;
        const bool selected = remove ? mSelected.isNull() :
            mItems[row - 1].mCapture.mId == mSelected;
        if (selected) gl_rect_2d(rect, LLColor4(mAccentColor.mV[0], mAccentColor.mV[1],
                                                mAccentColor.mV[2], .18f), true);
        gl_rect_2d(rect, mBorderColor, false);
        if (remove)
        {
            drawText(mFont, "None / Remove VCam feed", 8, top - 22,
                     selected ? mAccentColor : mTextColor,
                     getRect().getWidth() - 16);
            continue;
        }
        const Item& item = mItems[row - 1];
        const LLRect picture(5, top - 3, 57, top - 32);
        const LLRectf& uv = item.mPublication.mCellUV;
        if (item.mPublication.mAtlas.mGeneration &&
            mPreviewService->bindAtlas(item.mPublication.mAtlas))
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
            gl_rect_2d(picture, LLColor4(.08f, .09f, .11f, 1.f), true);
        gl_rect_2d(picture, mBorderColor, false);
        drawText(mFont, item.mLabel, 65, top - 22,
                 selected ? mAccentColor : mTextColor,
                 getRect().getWidth() - 73);
    }
    LLUICtrl::draw();
}

bool ALVCamTexturePickerList::handleMouseDown(S32, S32 y, MASK)
{
    const S32 row = rowAt(y);
    if (row < 0 || !mCount) return false;
    LLSD value;
    value["action"] = row == 0 ? "remove" : "capture";
    if (row > 0) value["capture"] = mItems[row - 1].mCapture.mId;
    setValue(value);
    onCommit();
    return true;
}

ALVCamProjectorList::ALVCamProjectorList(const Params& p)
    : LLUICtrl(p),
      mFont(LLFontGL::getFontSansSerifSmall()),
      mBorderColor(ALKitStyle::color(ALKitStyle::CARD_BORDER).get()),
      mTextColor(ALKitStyle::color(ALKitStyle::TEXT).get()),
      mMutedColor(ALKitStyle::color(ALKitStyle::MUTED).get()),
      mAccentColor(ALKitStyle::color(ALKitStyle::ACCENT).get())
{
    setMouseOpaque(true);
}

void ALVCamProjectorList::setItems(const std::vector<Item>& items)
{
    mItems = items;
    setScrollOffset(mScrollOffset);
}

S32 ALVCamProjectorList::rowAt(S32 y) const
{
    const S32 row = mScrollOffset +
        (getRect().getHeight() - y) / PROJECTOR_ROW_HEIGHT;
    return row >= 0 && row < static_cast<S32>(mItems.size()) ? row : -1;
}

void ALVCamProjectorList::setScrollOffset(S32 offset)
{
    const S32 visible_rows = llmax(1, getRect().getHeight() / PROJECTOR_ROW_HEIGHT);
    mScrollOffset = llclamp(offset, 0,
        llmax(0, static_cast<S32>(mItems.size()) - visible_rows));
}

void ALVCamProjectorList::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    const S32 paint_rows = llmax(1,
        (getRect().getHeight() + PROJECTOR_ROW_HEIGHT - 1) / PROJECTOR_ROW_HEIGHT);
    const S32 visible_rows = llmax(1, getRect().getHeight() / PROJECTOR_ROW_HEIGHT);
    const S32 end = llmin(static_cast<S32>(mItems.size()),
                          mScrollOffset + paint_rows);
    const S32 content_right = getRect().getWidth() - PROJECTOR_SCROLLBAR_WIDTH - 2;
    for (S32 row = mScrollOffset; row < end; ++row)
    {
        const S32 top = getRect().getHeight() -
            (row - mScrollOffset) * PROJECTOR_ROW_HEIGHT;
        const LLRect rect(0, top, content_right, top - PROJECTOR_ROW_HEIGHT);
        if (mItems[row].mSelected)
            gl_rect_2d(rect, LLColor4(mAccentColor.mV[0], mAccentColor.mV[1],
                                      mAccentColor.mV[2], .18f), true);
        gl_rect_2d(rect, mItems[row].mSelected ? mAccentColor : mBorderColor, false);
        drawText(mFont, mItems[row].mName, 7, top - 15, mTextColor,
                 llmax(1, content_right - 78));
        drawText(mFont, mItems[row].mMeta, 7, top - 29, mMutedColor,
                 llmax(1, content_right - 78));
        drawText(mFont, "Remove", content_right - 61, top - 22,
                 mTextColor, 55);
    }
    if (static_cast<S32>(mItems.size()) > visible_rows)
    {
        const S32 track_left = getRect().getWidth() - PROJECTOR_SCROLLBAR_WIDTH;
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
    LLUICtrl::draw();
}

bool ALVCamProjectorList::handleMouseDown(S32 x, S32 y, MASK)
{
    const S32 content_right = getRect().getWidth() - PROJECTOR_SCROLLBAR_WIDTH - 2;
    const S32 visible_rows = llmax(1, getRect().getHeight() / PROJECTOR_ROW_HEIGHT);
    if (x >= getRect().getWidth() - PROJECTOR_SCROLLBAR_WIDTH &&
        static_cast<S32>(mItems.size()) > visible_rows)
    {
        const S32 max_offset = llmax(0, static_cast<S32>(mItems.size()) - visible_rows);
        setScrollOffset((getRect().getHeight() - y) * max_offset /
                        llmax(1, getRect().getHeight()));
        return true;
    }
    const S32 row = rowAt(y);
    if (row < 0) return false;
    LLSD value;
    value["action"] = x >= content_right - 70 ? "remove" : "select";
    value["projector"] = mItems[row].mHandle.mId;
    setValue(value);
    onCommit();
    return true;
}

bool ALVCamProjectorList::handleScrollWheel(S32, S32, LLScrollDelta delta)
{
    const S32 before = mScrollOffset;
    setScrollOffset(mScrollOffset + delta.mClicks);
    if (mScrollOffset == before) return false;
    LLSliderWheel::noteScroll(this);
    return true;
}
