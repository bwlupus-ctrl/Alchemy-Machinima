/**
 * @file alpanelavatarlightresponse.cpp
 * @brief [AvatarLightResponse] Shared panel_avatar_light_response.xml: per-avatar Diffuse / Tame reflections /
 *        Brightness / Glow trim, target list, presets and the instrument readout.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelavatarlightresponse.h"

#include "alpanelcinecamparams.h"

#include "alghoststudio.h"
#include "alkitrow.h"
#include "llagentdata.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcharacter.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llworld.h"

#include <algorithm>

namespace
{
const char* const ALR_DOT_FILLED = "\xE2\x97\x8F";   // U+25CF
const char* const ALR_DOT_HOLLOW = "\xE2\x97\x8B";   // U+25CB

const LLColor4 ALR_COLOR_GREEN(0.30f, 0.90f, 0.30f, 1.f);
const LLColor4 ALR_COLOR_AMBER(1.00f, 0.70f, 0.10f, 1.f);
const LLColor4 ALR_COLOR_GREY(0.65f, 0.65f, 0.65f, 1.f);

const char* kindLabel(ALAvatarLightResponse::EKind kind)
{
    switch (kind)
    {
    case ALAvatarLightResponse::KIND_SELF:    return "You";
    case ALAvatarLightResponse::KIND_ANIMESH: return "Animesh";
    case ALAvatarLightResponse::KIND_CLONE:   return "Clone";
    default:                                  return "Resident";
    }
}

std::string shortId(const LLUUID& key)
{
    return key.asString().substr(0, 8);
}
}

static LLPanelInjector<ALPanelAvatarLightResponse> t_panel_avatar_light_response("panel_avatar_light_response");

bool ALPanelAvatarLightResponse::mHaveClip = false;
U64 ALPanelAvatarLightResponse::mClipRevision = 0;
ALLightResponse::Params ALPanelAvatarLightResponse::mClip;

ALPanelAvatarLightResponse::ALPanelAvatarLightResponse()
    : LLPanel()
{
    alRegisterMachinimaResetControl();
}

ALPanelAvatarLightResponse::~ALPanelAvatarLightResponse()
{
    if (mSavePending && ALAvatarLightResponse::instanceExists())
    {
        // Destroyed inside a pending debounce: the store already holds the edit, so write it now.
        mSavePending = false;
        ALAvatarLightResponse::instance().flushIfDirty();
    }
    if (mIdleRegistered)
    {
        gIdleCallbacks.deleteFunction(onIdle, this);
    }
    mChangeConnection.disconnect();
    mSavedConnection.disconnect();
}

bool ALPanelAvatarLightResponse::postBuild()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelAvatarLightResponse postBuild");
    mTargetList      = getChild<LLScrollListCtrl>("target_list");
    mOnlyAdjusted    = getChild<LLCheckBoxCtrl>("chk_only_adjusted");
    mDiffuse         = getChild<LLSliderCtrl>("sld_diffuse");
    mTame            = getChild<LLSliderCtrl>("sld_tame");
    mBrightness      = getChild<LLSliderCtrl>("sld_brightness");
    mGlow            = getChild<LLSliderCtrl>("sld_glow");
    mBypass          = getChild<LLCheckBoxCtrl>("chk_bypass");
    mSilAmount       = getChild<LLSliderCtrl>("sld_sil_amount");
    mSilFill         = getChild<LLColorSwatchCtrl>("swatch_sil_fill");
    mSilFillPreset   = getChild<LLComboBox>("combo_sil_fill");
    mSilHaze         = getChild<LLCheckBoxCtrl>("chk_sil_haze");
    mSilKeepRim      = getChild<LLCheckBoxCtrl>("chk_sil_keep_rim");
    mSilNote         = getChild<LLTextBox>("txt_sil_note");
    mPreset          = getChild<LLComboBox>("combo_preset");
    mApplyPreset     = getChild<LLButton>("btn_apply_preset");
    mResetSelected   = getChild<LLButton>("btn_reset_selected");
    mInheritSelected = getChild<LLButton>("btn_inherit_selected");
    mCopy            = getChild<LLButton>("btn_copy");
    mPaste           = getChild<LLButton>("btn_paste");
    mBrightnessValue = getChild<LLTextBox>("txt_brightness_ev");
    mStatus          = getChild<LLTextBox>("txt_status");
    mBudget          = getChild<LLTextBox>("txt_budget");

    // Hide through the kit row so the first reveal also reflows its card.
    ALKitRow::setControlVisible(mSilNote, false);
    ALKitRow::setControlVisible(mBudget, false);

    mTargetList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSelectionChanged(); });
    mTargetList->setCommitOnSelectionChange(true);
    mOnlyAdjusted->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        refreshRows(true);
        refreshEditors();
    });
    getChild<LLButton>("btn_refresh")->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        // A manual refresh is ordinary list work, not a reveal or structural
        // event. Coalesce it with store notifications at the 1 Hz cadence.
        mIdleRowsPending = true;
        mIdleStatePending = true;
    });

    mDiffuse->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_DIFFUSE); });
    mTame->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_TAME); });
    mBrightness->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_BRIGHTNESS); });
    mGlow->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_GLOW); });
    mSilAmount->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSliderCommit(FIELD_SIL_AMOUNT); });
    // Drag tracking: the 1 Hz row refresh must not reset a slider the user is holding.
    mDiffuse->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mTame->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mBrightness->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mGlow->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mSilAmount->setSliderMouseDownCallback([this](LLUICtrl*, const LLSD&) { mSliderDragging = true; });
    mDiffuse->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });
    mTame->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });
    mBrightness->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });
    mGlow->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });
    mSilAmount->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { onSliderMouseUp(); });

    mBypass->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBypass(); });
    mSilFill->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSilFill(false, true); });
    mSilFill->setOnSelectCallback([this](LLUICtrl*, const LLSD&) { onSilFill(true, true); });
    mSilFill->setOnCancelCallback([this](LLUICtrl*, const LLSD&) { onSilFill(true, false); });
    mSilFillPreset->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSilFillPreset(); });
    mSilHaze->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSilCheckbox(true); });
    mSilKeepRim->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSilCheckbox(false); });
    mApplyPreset->setCommitCallback([this](LLUICtrl*, const LLSD&) { onApplyPreset(); });
    mResetSelected->setCommitCallback([this](LLUICtrl*, const LLSD&) { onResetSelected(); });
    mInheritSelected->setCommitCallback([this](LLUICtrl*, const LLSD&) { onInheritSelected(); });
    // Director kit controls have page-unique names; standalone and v1 keep theirs. Probe by presence:
    // postBuild runs before the Director binds the page, so no flag is set yet.
    const char* reset_all_name = "btn_reset_all";
    if (findChild<LLButton>("la_btn_reset_all"))
    {
        reset_all_name = "la_btn_reset_all";
    }
    getChild<LLButton>(reset_all_name)->setCommitCallback([this](LLUICtrl*, const LLSD&) { onResetAll(); });
    mCopy->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCopy(); });
    mPaste->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPaste(); });

    mPreset->selectFirstItem();   // never leave the combo empty: an empty combo reads as index 0 (Reset)
    mChangeConnection = ALAvatarLightResponse::instance().setChangeCallback([this]()
    {
        mIdleStatePending = true;
        mIdleEditorsPending = true;
    });
    // Whoever writes the file (quit, close, another panel), this edit is on disk: drop the pending debounce so
    // the same edit is never written twice.
    mSavedConnection = ALAvatarLightResponse::instance().setSavedCallback([this]() { mSavePending = false; });
    if (isInVisibleChain())
    {
        gIdleCallbacks.addFunction(onIdle, this);
        mIdleRegistered = true;
    }

    refreshRows(true);
    refreshEditors();
    refreshStatus();
    return LLPanel::postBuild();
}

void ALPanelAvatarLightResponse::focusTarget(const LLUUID& target)
{
    if (target.notNull())
    {
        mPinnedKeys.insert(target);
        refreshRows(true);
        selectKeys(std::vector<LLUUID>(1, target));
    }
    else
    {
        refreshRows(true);
    }
    refreshEditors();
    refreshStatus();
}

void ALPanelAvatarLightResponse::onVisibilityChange(bool new_visibility)
{
    if (new_visibility && !mIdleRegistered && mTargetList)
    {
        gIdleCallbacks.addFunction(onIdle, this);
        mIdleRegistered = true;
    }
    if (!new_visibility && mSavePending && ALAvatarLightResponse::instanceExists())
    {
        // Hiding ends the debounce window: flush now rather than wait for an idle tick that may never come.
        mSavePending = false;
        ALAvatarLightResponse::instance().flushIfDirty();
    }
    if (new_visibility)
    {
        mStoreGate.reset();
        mIdleRowsPending = true;
        mIdleRowsImmediate = true;
        mIdleStatePending = true;
        mIdleEditorsPending = true;
    }
    LLPanel::onVisibilityChange(new_visibility);
}

void ALPanelAvatarLightResponse::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelAvatarLightResponse draw");
    if (!mIdleRegistered)
    {
        gIdleCallbacks.addFunction(onIdle, this);
        mIdleRegistered = true;
    }
    const bool tick = mRefreshTimer.getElapsedTimeF32() >= 1.f;
    if (tick)
    {
        mIdleRowsPending = true;
        mIdleStatePending = true;
    }
    if (mStoreGate.changed({ ALAvatarLightResponse::instance().revision() }))
    {
        mIdleStatePending = true;
    }
    LLPanel::draw();
}

// static
void ALPanelAvatarLightResponse::onIdle(void* user_data)
{
    static_cast<ALPanelAvatarLightResponse*>(user_data)->idleUpdate();
}

void ALPanelAvatarLightResponse::idleUpdate()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightResponse idleUpdate");
    releaseLostSliderDrag();
    if (!isWidgetOnScreen(this))
    {
        // Single-instance floaters retain their children after close, so a
        // child visibility callback is not guaranteed. Retire the callback
        // here; the first visible draw registers it again. Flush any pending
        // save now: nothing else will tick this panel until it is shown again.
        gIdleCallbacks.deleteFunction(onIdle, this);
        mIdleRegistered = false;
        if (mSavePending)
        {
            mSavePending = false;
            ALAvatarLightResponse::instance().flushIfDirty();
        }
        return;
    }
    if (mSeenClipRevision != mClipRevision)
    {
        mSeenClipRevision = mClipRevision;
        mIdleEditorsPending = true;
    }
    // Capture both decisions before refreshRows() resets the shared cadence
    // timer. Store callbacks can fire every slider event; all pending row,
    // state-cell and status work is therefore coalesced to at most 1 Hz.
    // Reveal and structural work marks rows immediate and may update both now.
    const bool cadence_due = mRefreshTimer.getElapsedTimeF32() >= 1.f;
    const bool immediate = mIdleRowsImmediate;
    bool rows_refreshed = false;
    if (mIdleRowsPending && (immediate || cadence_due))
    {
        refreshRows(false);
        rows_refreshed = true;
    }
    if (mIdleStatePending && (immediate || cadence_due))
    {
        mIdleStatePending = false;
        refreshStateCells();
        refreshStatus();
        refreshSilhouetteNote();
        if (!rows_refreshed)
        {
            mRefreshTimer.reset();
        }
    }
    const bool slider_being_edited = alIsBeingEdited(mDiffuse)
        || alIsBeingEdited(mTame)
        || alIsBeingEdited(mBrightness)
        || alIsBeingEdited(mGlow)
        || alIsBeingEdited(mSilAmount);
    if (mIdleEditorsPending && !slider_being_edited)
    {
        refreshEditors();
        setEnabledIfChanged(mPaste, !selectedKeys().empty() && mHaveClip);
        mIdleEditorsPending = false;
    }
    if (mSavePending && !mSliderDragging && mSaveDebounce.getElapsedTimeF32() >= 0.5f)
    {
        mSavePending = false;
        ALAvatarLightResponse::instance().flushIfDirty();
    }
}

std::string ALPanelAvatarLightResponse::rowName(const LLUUID& key, ALAvatarLightResponse::EKind kind,
                                                  bool& resolved) const
{
    resolved = true;
    switch (kind)
    {
    case ALAvatarLightResponse::KIND_SELF:
        return "You";
    case ALAvatarLightResponse::KIND_CLONE:
    {
        const ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(key);
        if (inst)
        {
            if (!inst->mName.empty())
            {
                return inst->mName;
            }
            if (!inst->mSourceLabel.empty())
            {
                return inst->mSourceLabel;
            }
        }
        return "Clone " + shortId(key);
    }
    case ALAvatarLightResponse::KIND_ANIMESH:
        return "Animesh " + shortId(key);
    default:
        break;
    }
    LLAvatarName av_name;
    if (LLAvatarNameCache::get(key, &av_name))
    {
        return av_name.getDisplayName();
    }
    resolved = false;
    return shortId(key);
}

void ALPanelAvatarLightResponse::collectRows(std::vector<LLUUID>& order,
                                               std::map<LLUUID, RowInfo>& info) const
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightResponse collectRows");
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();

    auto add = [&](const LLUUID& key, ALAvatarLightResponse::EKind kind, const std::string& fallback_name)
    {
        if (key.isNull() || info.find(key) != info.end())
        {
            return;
        }
        RowInfo row;
        row.mKind = kind;
        bool resolved = true;
        row.mName = rowName(key, kind, resolved);
        if (!resolved && !fallback_name.empty())
        {
            row.mName = fallback_name;
        }
        info[key] = row;
        order.push_back(key);
    };

    // Canonical Director identities (You first, including any subject letters).
    for (const LLDirectorCast::ActorChoice& choice :
         LLDirectorCast::instance().getActorChoices())
    {
        if (LLDirectorCast::isSelf(choice.mId))
        {
            add(gAgentID, ALAvatarLightResponse::KIND_SELF, choice.mLabel);
            continue;
        }
        ALAvatarLightResponse::EKind kind = ALAvatarLightResponse::KIND_RESIDENT;
        LLViewerObject* obj = gObjectList.findObject(choice.mId);
        LLVOAvatar* avatar = obj ? obj->asAvatar() : nullptr;
        if (obj && (!avatar || avatar->isControlAvatar()))
        {
            kind = ALAvatarLightResponse::KIND_ANIMESH;
        }
        add(choice.mId, kind, choice.mLabel);
    }

    // Residents in draw distance
    uuid_vec_t resident_ids;
    if (LLWorld::instanceExists())
    {
        LLWorld::getInstance()->getAvatars(&resident_ids);
    }
    for (const LLUUID& id : resident_ids)
    {
        if (id != gAgentID)
        {
            add(id, ALAvatarLightResponse::KIND_RESIDENT, std::string());
        }
    }

    // Animesh in view (control avatars keyed by their linkset root)
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* av = static_cast<LLVOAvatar*>(character);
        if (av && av->isControlAvatar() && !av->isDead())
        {
            add(av->getActorFxOwnerId(), ALAvatarLightResponse::KIND_ANIMESH, std::string());
        }
    }

    // Ghost Studio instances (overlay and entity clones)
    for (const ALGhostStudio::Instance& inst : ALGhostStudio::instance().getInstances())
    {
        add(inst.mId, ALAvatarLightResponse::KIND_CLONE, std::string());
    }

    // Requested-by-menu targets and stored entries that are not present
    for (const LLUUID& key : mPinnedKeys)
    {
        add(key, ALAvatarLightResponse::KIND_RESIDENT, std::string());
    }
    for (const auto& kv : store.entries())
    {
        if (info.find(kv.first) == info.end())
        {
            add(kv.first, kv.second.mKind, kv.second.mLabel);
            std::map<LLUUID, RowInfo>::iterator it = info.find(kv.first);
            if (it != info.end())
            {
                it->second.mKind = kv.second.mKind;
                it->second.mAway = true;
                if (!kv.second.mLabel.empty())
                {
                    it->second.mName = kv.second.mLabel;
                }
            }
        }
    }
}

// ui-draw-lint: allow-function(raw-write) cell updates compare old values and row rebuild runs only when membership changes
void ALPanelAvatarLightResponse::refreshRows(bool force)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightResponse refreshRows");
    if (!mTargetList)
    {
        return;
    }
    // Row discovery/rebuild is the >50-row path: coalesce ordinary requests to
    // 1 Hz. Reveal and known structural changes set mIdleRowsImmediate.
    mIdleRowsPending = false;
    mIdleRowsImmediate = false;
    mRefreshTimer.reset();
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();

    std::vector<LLUUID> order;
    std::map<LLUUID, RowInfo> info;
    collectRows(order, info);

    if (mOnlyAdjusted->get())
    {
        std::vector<LLUUID> filtered;
        for (const LLUUID& key : order)
        {
            const ALAvatarLightResponse::Entry* entry = store.findEntry(key);
            if (entry && !ALLightResponse::isIdentity(entry->mParams))
            {
                filtered.push_back(key);
            }
        }
        order.swap(filtered);
    }
#if LL_PROFILER_CATEGORY_ENABLE_UI
    LL_PROFILE_ZONE_NUM(order.size());
#endif

    const std::set<LLUUID> wanted(order.begin(), order.end());
    const std::set<LLUUID> shown(mRowKeys.begin(), mRowKeys.end());
    if (wanted == shown)
    {
        // Same rows: names, away flags and kind can still change in place.
        mRowInfo = info;
        for (LLScrollListItem* item : mTargetList->getAllData())
        {
            const std::map<LLUUID, RowInfo>::const_iterator it = info.find(item->getUUID());
            if (it == info.end())
            {
                continue;
            }
            LLScrollListCell* name_cell = item->getColumn(1);
            const std::string name = it->second.mName + (it->second.mAway ? " (away)" : "");
            if (name_cell && name_cell->getValue().asString() != name)
            {
                name_cell->setValue(name);
            }
            LLScrollListCell* kind_cell = item->getColumn(2);
            const std::string kind = kindLabel(it->second.mKind);
            if (kind_cell && kind_cell->getValue().asString() != kind)
            {
                kind_cell->setValue(kind);
            }
        }
        if (force)
        {
            refreshStateCells();
        }
        return;
    }

    const std::vector<LLUUID> keep_selected = selectedKeys();
    const S32 scroll_pos = mTargetList->getScrollPos();

    mUpdating = true;
    // A real membership change is uncommon and retains the old full-rebuild
    // order: You, cast, nearby, animesh, clones, pinned, then away.
    mTargetList->deleteAllItems();
    mStateVisuals.clear();
    const char* name_column = mTargetList->getColumn("la_name") ? "la_name" : "name";
    for (const LLUUID& key : order)
    {
        const RowInfo& row = info[key];
        LLSD element;
        element["value"] = key;
        element["columns"][0]["column"] = "state";
        element["columns"][0]["value"] = "";
        element["columns"][1]["column"] = name_column;
        element["columns"][1]["value"] = row.mName + (row.mAway ? " (away)" : "");
        element["columns"][2]["column"] = "kind";
        element["columns"][2]["value"] = kindLabel(row.mKind);
        mTargetList->addElement(element, ADD_BOTTOM);
    }
    mRowKeys = order;
    mRowInfo = info;
    selectKeys(keep_selected);
    mTargetList->setScrollPos(scroll_pos);
    mUpdating = false;

    refreshStateCells();
    if (!mSliderDragging)
    {
        refreshEditors();
    }
}

// ui-draw-lint: allow-function(raw-write) state-cell writes are gated by the cached visual state
void ALPanelAvatarLightResponse::refreshStateCells()
{
    if (!mTargetList)
    {
        return;
    }
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();
    for (LLScrollListItem* item : mTargetList->getAllData())
    {
        const LLUUID key = item->getUUID();
        std::string text;
        LLColor4 color = ALR_COLOR_GREY;
        S32 visual = 0;
        const ALAvatarLightResponse::Entry* entry = store.findEntry(key);
        if (entry)
        {
            if (ALLightResponse::isIdentity(entry->mParams))
            {
                text = ALR_DOT_HOLLOW;
                visual = 1;
            }
            else
            {
                const ALAvatarLightResponse::TargetStatus st = store.queryStatus(key);
                const bool ok = st.mUploads > 0 && st.mVisible == 1 && !st.mMuted;   // unknown (-1) stays amber
                text = ALR_DOT_FILLED;
                color = ok ? ALR_COLOR_GREEN : ALR_COLOR_AMBER;
                visual = ok ? 3 : 2;
            }
        }
        if (mStateVisuals[key] != visual)
        {
            mStateVisuals[key] = visual;
            if (LLScrollListCell* cell = item->getColumn(0))
            {
                cell->setValue(text);
                cell->setColor(color);
            }
        }
    }
}

void ALPanelAvatarLightResponse::refreshStatus()
{
    if (!mStatus)
    {
        return;
    }
    ALAvatarLightResponse& store = ALAvatarLightResponse::instance();
    S32 adjusted = 0;
    S32 uploaded = 0;
    S32 visible = 0;
    for (const auto& kv : store.entries())
    {
        if (ALLightResponse::isIdentity(kv.second.mParams))
        {
            continue;
        }
        ++adjusted;
        const ALAvatarLightResponse::TargetStatus st = store.queryStatus(kv.first);
        if (st.mUploads > 0)
        {
            ++uploaded;
        }
        if (st.mVisible == 1)
        {
            ++visible;
        }
    }
    setTextIfChanged(mStatus,
                     llformat("%d adjusted  |  %d uploaded  |  %d visible", adjusted, uploaded, visible));

    const S32 over = store.countLiveOverBudget();
    if (over > 0)
    {
        const bool changed = setTextIfChanged(
            mBudget,
            llformat("%d adjusted avatars render at full detail beyond your avatar impostor limit (%d) - expect lower FPS.",
                     over, static_cast<S32>(LLVOAvatar::sMaxNonImpostors)));
        if (changed)
        {
            mBudget->setColor(ALR_COLOR_AMBER);
        }
        ALKitRow::setControlVisible(mBudget, true);
    }
    else
    {
        ALKitRow::setControlVisible(mBudget, false);
    }
}

std::vector<LLUUID> ALPanelAvatarLightResponse::selectedKeys() const
{
    std::vector<LLUUID> keys;
    if (mTargetList)
    {
        for (LLScrollListItem* item : mTargetList->getAllSelected())
        {
            keys.push_back(item->getUUID());
        }
    }
    return keys;
}

void ALPanelAvatarLightResponse::selectKeys(const std::vector<LLUUID>& keys)
{
    if (!mTargetList)
    {
        return;
    }
    const bool was_updating = mUpdating;
    mUpdating = true;
    // One multi-select pass. setSelectedByValue(key, true) is single-select by default and
    // would deselect every key added before it.
    mTargetList->deselectAllItems(true);
    uuid_vec_t ids(keys.begin(), keys.end());
    mTargetList->selectMultiple(ids);
    mUpdating = was_updating;
}

void ALPanelAvatarLightResponse::deferSave()
{
    if (!mIdleRegistered)
    {
        // A hidden page retires its idle callback, so the 0.5 s debounce would never fire.
        mSavePending = false;
        ALAvatarLightResponse::instance().flushIfDirty();
        return;
    }
    mSavePending = true;
    mSaveDebounce.reset();
}

void ALPanelAvatarLightResponse::releaseLostSliderDrag()
{
    if (!mSliderDragging)
    {
        return;
    }
    // Alt-tab or a window switch takes mouse capture away mid-drag, and the slider never reports its mouse-up.
    const LLView* const sliders[] = { mDiffuse, mTame, mBrightness, mGlow, mSilAmount };
    for (const LLView* slider : sliders)
    {
        if (slider && gFocusMgr.childHasMouseCapture(slider))
        {
            return;
        }
    }
    mSliderDragging = false;
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
}

ALLightResponse::Params ALPanelAvatarLightResponse::currentParams(const LLUUID& key) const
{
    const ALAvatarLightResponse::Entry* entry = ALAvatarLightResponse::instance().findEntry(key);
    return entry ? entry->mParams : ALLightResponse::Params();
}

void ALPanelAvatarLightResponse::applyParams(const LLUUID& key, const ALLightResponse::Params& params, bool save_now)
{
    const ALAvatarLightResponse::Entry* old_entry = ALAvatarLightResponse::instance().findEntry(key);
    const bool was_adjusted = old_entry && !ALLightResponse::isIdentity(old_entry->mParams);
    const bool will_be_adjusted = !ALLightResponse::isIdentity(ALLightResponse::sanitize(params));
    ALAvatarLightResponse::EKind kind = ALAvatarLightResponse::KIND_RESIDENT;
    std::string label = shortId(key);
    const std::map<LLUUID, RowInfo>::const_iterator it = mRowInfo.find(key);
    if (it != mRowInfo.end())
    {
        kind = it->second.mKind;
        if (!it->second.mName.empty())
        {
            label = it->second.mName;
        }
    }
    else if (const ALAvatarLightResponse::Entry* entry = ALAvatarLightResponse::instance().findEntry(key))
    {
        kind = entry->mKind;
        label = entry->mLabel;
    }
    if (!ALAvatarLightResponse::instance().set(key, kind, label, params, save_now))
    {
        // capacity reached: refuse loudly (unique notification, so a slider drag cannot stack them)
        LLNotificationsUtil::add("AvatarLightResponseCapReached",
            LLSD().with("MAX", static_cast<LLSD::Integer>(ALAvatarLightResponse::maxStoredEntries())));
    }
    else if (mOnlyAdjusted->get() && was_adjusted != will_be_adjusted)
    {
        // This edit changes list membership. Defer until the current callback
        // unwinds, but bypass the 1 Hz limiter as a structural change.
        mIdleRowsPending = true;
        mIdleRowsImmediate = true;
    }
}

void ALPanelAvatarLightResponse::refreshEditors()
{
    if (!mTargetList)
    {
        return;
    }
    const std::vector<LLUUID> keys = selectedKeys();
    const bool any = !keys.empty();
    const ALLightResponse::Params p = any ? currentParams(keys.front()) : ALLightResponse::Params();

    mUpdating = true;
    setValueIfChanged(mDiffuse, LLSD((F64)(p.mDiffuse * 100.f)), true);
    setValueIfChanged(mTame, LLSD((F64)(p.mTame * 100.f)), true);
    setValueIfChanged(mBrightness, LLSD((F64)p.mExposureEV), true);
    setValueIfChanged(mGlow, LLSD((F64)(p.mGlow * 100.f)), true);
    setValueIfChanged(mBypass, LLSD(p.mBypass), true);
    setValueIfChanged(mSilAmount, LLSD((F64)(p.mSilAmount * 100.f)), true);
    setValueIfChanged(mSilFill,
                      LLColor4(p.mSilFillSrgb.mV[VX], p.mSilFillSrgb.mV[VY],
                               p.mSilFillSrgb.mV[VZ], 1.f).getValue(), true);
    setValueIfChanged(mSilFillPreset,
                      LLSD((S32)ALLightResponse::matchFillPreset(p.mSilFillSrgb)), true);
    setValueIfChanged(mSilHaze, LLSD(p.mSilHaze), true);
    setValueIfChanged(mSilKeepRim, LLSD(p.mSilKeepRim), true);
    mUpdating = false;
    refreshBrightnessValue();
    refreshSilhouetteNote();

    setEnabledIfChanged(mDiffuse, any);
    setEnabledIfChanged(mTame, any);
    setEnabledIfChanged(mBrightness, any);
    setEnabledIfChanged(mGlow, any);
    setEnabledIfChanged(mBypass, any);
    setEnabledIfChanged(mSilAmount, any);
    setEnabledIfChanged(mSilFill, any);
    setEnabledIfChanged(mSilFillPreset, any);
    setEnabledIfChanged(mSilHaze, any);
    setEnabledIfChanged(mSilKeepRim, any);
    setEnabledIfChanged(mPreset, any);
    setEnabledIfChanged(mApplyPreset, any);
    setEnabledIfChanged(mResetSelected, any);
    setEnabledIfChanged(mCopy, any);
    setEnabledIfChanged(mPaste, any && mHaveClip);

    // Inherit is meaningful only where a fallback exists (animesh -> wearer, clone -> source)
    // and only for rows that currently hold an explicit entry.
    bool can_inherit = any;
    for (const LLUUID& key : keys)
    {
        const ALAvatarLightResponse::Entry* entry = ALAvatarLightResponse::instance().findEntry(key);
        const std::map<LLUUID, RowInfo>::const_iterator it = mRowInfo.find(key);
        const bool fallback_kind = it != mRowInfo.end()
            && (it->second.mKind == ALAvatarLightResponse::KIND_ANIMESH
                || it->second.mKind == ALAvatarLightResponse::KIND_CLONE);
        if (!entry || !fallback_kind)
        {
            can_inherit = false;
            break;
        }
    }
    setEnabledIfChanged(mInheritSelected, can_inherit);
}

void ALPanelAvatarLightResponse::refreshBrightnessValue()
{
    if (mBrightnessValue && mBrightness)
    {
        setTextIfChanged(mBrightnessValue, llformat("%+.2f EV", mBrightness->getValueF32()));
    }
}

void ALPanelAvatarLightResponse::refreshSilhouetteNote()
{
    static LLCachedControl<bool> emissive_buffer(
        gSavedSettings, "RenderEnableEmissiveBuffer", false);
    static LLCachedControl<bool> force_emissive(
        gSavedSettings, "AvatarSilhouetteForceEmissive", false);
    static LLCachedControl<bool> rig_rim_enabled(
        gSavedSettings, "CineRigRimEnabled", false);
    if (!mSilNote)
    {
        return;
    }

    bool any_silhouette = false;
    for (const auto& kv : ALAvatarLightResponse::instance().entries())
    {
        const ALLightResponse::Params p = ALLightResponse::sanitize(kv.second.mParams);
        if (!p.mBypass && p.mSilAmount > ALLightResponse::SILHOUETTE_EPSILON)
        {
            any_silhouette = true;
            break;
        }
    }
    const bool preparing = any_silhouette
        && !emissive_buffer
        && !force_emissive;

    bool rim_unavailable = false;
    const std::vector<LLUUID> keys = selectedKeys();
    if (!keys.empty())
    {
        const ALLightResponse::Params p = currentParams(keys.front());
        rim_unavailable = p.mSilAmount > ALLightResponse::SILHOUETTE_EPSILON
            && p.mSilKeepRim && !rig_rim_enabled;
    }

    const std::string note = preparing
        ? "Preparing silhouette buffer\xE2\x80\xA6"
        : (rim_unavailable ? "Keep Rim: Rig Rim is off (Cine Light Rig)" : std::string());
    setTextIfChanged(mSilNote, note);
    ALKitRow::setControlVisible(mSilNote, !note.empty());
}

void ALPanelAvatarLightResponse::onSelectionChanged()
{
    if (mUpdating)
    {
        return;
    }
    refreshEditors();
}

void ALPanelAvatarLightResponse::onSliderCommit(EField field)
{
    if (mUpdating)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        ALLightResponse::Params p = currentParams(key);
        switch (field)
        {
        case FIELD_DIFFUSE:    p.mDiffuse = mDiffuse->getValueF32() / 100.f; break;
        case FIELD_TAME:       p.mTame = mTame->getValueF32() / 100.f; break;
        case FIELD_BRIGHTNESS: p.mExposureEV = mBrightness->getValueF32(); break;
        case FIELD_GLOW:       p.mGlow = mGlow->getValueF32() / 100.f; break;
        case FIELD_SIL_AMOUNT: p.mSilAmount = mSilAmount->getValueF32() / 100.f; break;
        default: break;
        }
        applyParams(key, p, false);   // saved on slider mouse-up
    }
    deferSave();
    refreshBrightnessValue();
    if (field == FIELD_SIL_AMOUNT)
    {
        refreshSilhouetteNote();
    }
}

void ALPanelAvatarLightResponse::onSliderMouseUp()
{
    mSliderDragging = false;
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
}

void ALPanelAvatarLightResponse::onSilFill(bool save_now, bool custom)
{
    if (mUpdating)
    {
        return;
    }
    const LLColor4 color = mSilFill->get();
    const LLVector3 fill(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE]);
    for (const LLUUID& key : selectedKeys())
    {
        ALLightResponse::Params p = currentParams(key);
        p.mSilFillSrgb = fill;
        applyParams(key, p, false);
    }
    if (save_now)
    {
        mSavePending = false;
        ALAvatarLightResponse::instance().flushIfDirty();
    }
    else
    {
        deferSave();
    }
    if (custom)
    {
        setValueIfChanged(mSilFillPreset, LLSD((S32)ALLightResponse::FILL_CUSTOM), true);
    }
    else
    {
        setValueIfChanged(mSilFillPreset,
                          LLSD((S32)ALLightResponse::matchFillPreset(fill)), true);
    }
    refreshSilhouetteNote();
}

void ALPanelAvatarLightResponse::onSilFillPreset()
{
    if (mUpdating)
    {
        return;
    }
    const S32 value = mSilFillPreset->getValue().asInteger();
    if (value < 0 || value >= ALLightResponse::FILL_PRESET_COUNT)
    {
        return;
    }
    const ALLightResponse::EFillPreset preset = static_cast<ALLightResponse::EFillPreset>(value);
    if (preset == ALLightResponse::FILL_CUSTOM)
    {
        return;
    }
    const LLVector3 fill = ALLightResponse::fillPresetSrgb(preset);
    for (const LLUUID& key : selectedKeys())
    {
        ALLightResponse::Params p = currentParams(key);
        p.mSilFillSrgb = fill;
        applyParams(key, p, false);
    }
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
    refreshEditors();
}

void ALPanelAvatarLightResponse::onSilCheckbox(bool haze)
{
    if (mUpdating)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        ALLightResponse::Params p = currentParams(key);
        if (haze)
        {
            p.mSilHaze = mSilHaze->get();
        }
        else
        {
            p.mSilKeepRim = mSilKeepRim->get();
        }
        applyParams(key, p, false);
    }
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
    refreshEditors();
}

void ALPanelAvatarLightResponse::onBypass()
{
    if (mUpdating)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        ALLightResponse::Params p = currentParams(key);
        p.mBypass = mBypass->get();
        applyParams(key, p, false);   // one write for the whole selection, below
    }
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
    refreshEditors();
}

void ALPanelAvatarLightResponse::onApplyPreset()
{
    if (mPreset->getCurrentIndex() < 0)
    {
        return;   // nothing chosen: never fall through to index 0 (Reset)
    }
    const S32 index = mPreset->getValue().asInteger();
    if (index < 0 || index >= ALLightResponse::PRESET_COUNT)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        applyParams(key,
                    ALLightResponse::applyPreset(currentParams(key), static_cast<ALLightResponse::EPreset>(index)),
                    false);
    }
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
    refreshEditors();
}

void ALPanelAvatarLightResponse::onResetSelected()
{
    // Explicit identity: still terminates the fallback chain (Inherit removes the entry instead).
    for (const LLUUID& key : selectedKeys())
    {
        applyParams(key, ALLightResponse::Params(), false);
    }
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
    refreshEditors();
}

void ALPanelAvatarLightResponse::onInheritSelected()
{
    for (const LLUUID& key : selectedKeys())
    {
        ALAvatarLightResponse::instance().inherit(key, false);
    }
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
    refreshRows(true);
    refreshEditors();
}

void ALPanelAvatarLightResponse::onResetAll()
{
    const LLHandle<ALPanelAvatarLightResponse> handle = getDerivedHandle<ALPanelAvatarLightResponse>();
    LLNotificationsUtil::add("AvatarLightResponseResetAll", LLSD(), LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
            {
                ALAvatarLightResponse::instance().clearAll(true, true);
                if (ALPanelAvatarLightResponse* self = handle.get())
                {
                    self->mIdleRowsPending = true;
                    self->mIdleRowsImmediate = true;
                }
            }
        });
}

void ALPanelAvatarLightResponse::onCopy()
{
    const std::vector<LLUUID> keys = selectedKeys();
    if (keys.empty())
    {
        return;
    }
    mClip = currentParams(keys.front());
    mHaveClip = true;
    ++mClipRevision;
    mSeenClipRevision = mClipRevision;
    refreshEditors();
}

void ALPanelAvatarLightResponse::onPaste()
{
    if (!mHaveClip)
    {
        return;
    }
    for (const LLUUID& key : selectedKeys())
    {
        applyParams(key, mClip, false);
    }
    mSavePending = false;
    ALAvatarLightResponse::instance().flushIfDirty();
    refreshEditors();
}
