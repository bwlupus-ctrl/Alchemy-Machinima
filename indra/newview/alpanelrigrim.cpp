#include "llviewerprecompiledheaders.h"
#include "alpresetlook.h"

#include "alpanelrigrim.h"

#include "alkitpresets.h"
#include "alcinelightrigmanager.h"
#include "alghoststudio.h"
#include "alkitpage.h"
#include "alkitshell.h"
#include "lldirectorcast.h"
#include "llmenugl.h"
#include "llcallbacklist.h"
#include "llfloaterreg.h"
#include "llfloater.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "llviewermenu.h"
#include "llpanel.h"

#include <algorithm>
#include <map>
#include <set>

namespace
{
const std::vector<std::string>& rimSettings()
{
    return ALPresetLook::owned(ALPresetLook::RIM_CUSTOM);
}

class RigRimPresetProvider final : public ALKitPresetProvider
{
public:
    RigRimPresetProvider() : mStore("rig_rim", rimSettings()) {}
    const std::string& key() const override { return mKey; }
    void               list(std::vector<Entry>& out) const override
    {
        for (const std::string& name : ALRigRimController::presetNames())
        {
            out.push_back({ "b:" + name, name, true, ALRigRimController::presetDescription(name) });
        }
        for (const std::string& name : mStore.names())
        {
            out.push_back({ "u:" + name, name, false });
        }
    }
    bool apply(const std::string& id) override
    {
        if (id.compare(0, 2, "b:") == 0)
        {
            return ALRigRimController::applyPresetByName(id.substr(2));
        }
        return id.compare(0, 2, "u:") == 0 && mStore.load(id.substr(2));
    }
    bool                            canSaveUser() const override { return true; }
    bool                            saveUser(const std::string& name) override { return mStore.save(name); }
    bool                            canDelete(const std::string& id) const override { return id.compare(0, 2, "u:") == 0; }
    bool                            deleteUser(const std::string& id) override { return canDelete(id) && mStore.remove(id.substr(2)); }
    U64                             revision() const override { return mStore.revision(); }
    const std::vector<std::string>& trackedSettings() const override { return rimSettings(); }

private:
    std::string                mKey = "rig_rim";
    mutable ALKitSnapshotStore mStore;
};

void registerProvider()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;
    ALKitPresetRegistry::instance().add(std::make_unique<RigRimPresetProvider>());
}
} // namespace

static LLPanelInjector<ALPanelRigRim> register_panel_rig_rim("panel_rig_rim");

ALPanelRigRim::ALPanelRigRim()
{ registerProvider(); }

ALPanelRigRim::~ALPanelRigRim()
{
    gIdleCallbacks.deleteFunction(idle, this);
}

bool ALPanelRigRim::postBuild()
{
    registerProvider();
    mRim.attach(this);
    mExtras.attach(this);
    gIdleCallbacks.addFunction(idle, this);
    return LLPanel::postBuild();
}

void ALPanelRigRim::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelRigRim draw");
    if (!isWidgetOnScreen(this))
    {
        return;
    }
    mRim.tickFrame();
    if (mRefreshClock.due())
    {
        mRim.tickRefresh();
    }
    LLPanel::draw();
}

void ALPanelRigRim::idle(void* data)
{
    auto* panel = static_cast<ALPanelRigRim*>(data);
    if (!panel->mExtraRefreshClock.due()) return;
    panel->mExtras.finishTrim();
    if (isWidgetOnScreen(panel))
        panel->mExtras.refresh();
}

void ALPanelRigRim::onVisibilityChange(bool visible)
{
    if (visible)
    {
        mRim.onReveal();
        mExtras.onReveal();
        mRefreshClock.forceDue();
        mExtraRefreshClock.forceDue();
    }
    LLPanel::onVisibilityChange(visible);
}

namespace
{
using ExtraRimRows = std::map<LLUUID, F32>;
ExtraRimRows extraRimRows(const LLSD& block)
{
    ExtraRimRows rows;
    for (const auto& row : llsd::inArray(block["items"]))
        rows[row["actor"].asUUID()] = (F32)row["trim"].asReal();
    return rows;
}
// Apply only this edit's delta. An unrelated extra or source change survives replay.
bool patchExtraRimDelta(LLSD& current, const LLSD& expected, const LLSD& target, std::string& reason)
{
    auto live = extraRimRows(current);
    const auto from = extraRimRows(expected), to = extraRimRows(target);
    if (expected["source"] != target["source"])
    {
        if (current["source"] != expected["source"])
        { reason = "Direction from changed elsewhere"; return false; }
        current["source"] = target["source"];
    }
    std::set<LLUUID> ids;
    for (const auto& row : from) ids.insert(row.first);
    for (const auto& row : to) ids.insert(row.first);
    for (const auto& id : ids)
    {
        const auto a = from.find(id), b = to.find(id);
        const auto c = live.find(id);
        if (a != from.end() && b != to.end() && a->second == b->second) continue;
        if ((a == from.end()) != (c == live.end()) ||
            (a != from.end() && a->second != c->second))
        { reason = "An edited extra rim changed elsewhere"; return false; }
        if (b == to.end()) live.erase(id);
        else
        {
            auto& rigs = ALCineLightRigManager::instance();
            const auto& studio = ALGhostStudio::instance();
            const auto* clone = studio.getInstance(id);
            if (!clone) clone = studio.findInstanceByRuntime(id);
            if (!LLDirectorCast::instance().getStoredMember(id) ||
                rigs.resolveBorrowActor(id, true) != id ||
                (clone && clone->mKind == ALGhostStudio::BACKING_OVERLAY))
            { reason = "An extra rim actor left the cast or became unavailable"; return false; }
            live[id] = b->second;
        }
    }
    if (live.size() > 4) { reason = "Four extra rims are already in use"; return false; }
    // Order determines mask overflow priority. Restore removed actors beside
    // their snapshot neighbors without reordering unrelated live actors.
    std::vector<LLUUID> order, target_order;
    for (const auto& row : llsd::inArray(current["items"]))
    {
        const LLUUID id = row["actor"].asUUID();
        if (live.count(id)) order.push_back(id);
    }
    for (const auto& row : llsd::inArray(target["items"]))
        target_order.push_back(row["actor"].asUUID());
    for (size_t i = 0; i < target_order.size(); ++i)
    {
        const LLUUID id = target_order[i];
        if (!live.count(id) || std::find(order.begin(), order.end(), id) != order.end()) continue;
        auto insertion = order.end();
        for (size_t next = i + 1; next < target_order.size(); ++next)
        {
            insertion = std::find(order.begin(), order.end(), target_order[next]);
            if (insertion != order.end()) break;
        }
        if (insertion == order.end())
            for (size_t previous = i; previous > 0; --previous)
            {
                const auto neighbor = std::find(order.begin(), order.end(), target_order[previous - 1]);
                if (neighbor != order.end()) { insertion = neighbor + 1; break; }
            }
        order.insert(insertion, id);
    }
    LLSD items = LLSD::emptyArray();
    for (const auto& id : order)
    {
        LLSD item; item["actor"] = id; item["trim"] = live.at(id);
        items.append(item);
    }
    current["items"] = items;
    return true;
}
void rewriteExtraRimSnapshot(LLSD& block, const LLUUID& old_id, const LLUUID& new_id)
{
    LLSD items = LLSD::emptyArray();
    for (auto row : llsd::inArray(block["items"]))
    {
        if (row["actor"].asUUID() == old_id)
        {
            if (new_id.isNull()) continue;
            row["actor"] = new_id;
        }
        items.append(row);
    }
    block["items"] = items;
}
}

namespace
{
void recordExtraRimSnapshot(ALKitShell* shell, LLView* page, const LLSD& before,
                            const LLSD& after, const std::string& label)
{
    if (before == after) return;
    auto edit = std::make_shared<std::pair<LLSD, LLSD>>(before, after);
    auto validate = [edit](bool redo, std::string& reason)
    {
        LLSD live = ALCineLightRigManager::instance().captureExtraRims();
        return patchExtraRimDelta(live, redo ? edit->first : edit->second,
                                 redo ? edit->second : edit->first, reason);
    };
    ALKitHistory::ScopedSource source(page);
    shell->history().recordCustom(page, "extra_rims", label,
        [edit](bool redo, std::string& reason)
        {
            LLSD live = ALCineLightRigManager::instance().captureExtraRims();
            if (!patchExtraRimDelta(live, redo ? edit->first : edit->second,
                                   redo ? edit->second : edit->first, reason)) return false;
            ALCineLightRigManager::instance().restoreExtraRims(live);
            return true;
        }, false, validate,
        [edit](const LLUUID& old_id, const LLUUID& new_id)
        {
            rewriteExtraRimSnapshot(edit->first, old_id, new_id);
            rewriteExtraRimSnapshot(edit->second, old_id, new_id);
        });
}
struct PendingExtraRimEdit
{
    LLSD before, after;
    std::string label;
};
struct PendingExtraRimHistory
{
    LLHandle<LLView> shell, page;
    U64 sceneEpoch = 0;
    std::vector<PendingExtraRimEdit> edits;
    // Keep rekey aliases alive until the queued snapshots acquire history hooks.
    std::vector<std::shared_ptr<const LLUUID>> actors;
};
std::map<ALKitShell*, std::shared_ptr<PendingExtraRimHistory>> pendingExtraRimHistory;
void resolveExtraRimSnapshot(LLSD& block)
{
    const LLSD items = block["items"];
    for (const auto& item : llsd::inArray(items))
    {
        const LLUUID id = item["actor"].asUUID();
        const LLUUID resolved = ALCineLightRigManager::instance().resolveBorrowActor(id, true);
        if (resolved != id) rewriteExtraRimSnapshot(block, id, resolved);
    }
}
}

void ALExtraRimUI::recordEdit(ALKitShell* shell, const LLSD& before, const std::string& label)
{
    if (!shell) return;
    const LLSD after = ALCineLightRigManager::instance().captureExtraRims();
    if (before == after) return;
    // Capture both ends now. A proxy's outer input scope belongs to Quick or
    // Favorites; record after it unwinds so the custom step belongs to Rim.
    LLPanel* page = shell->ensurePageBuilt("light_group/light_rim_tab");
    if (!page) return;
    auto* input = dynamic_cast<ALKitPage*>(ALKitHistory::inputPage());
    auto found = pendingExtraRimHistory.find(shell);
    if (found != pendingExtraRimHistory.end() &&
        (found->second->shell.get() != shell || found->second->sceneEpoch !=
         ALCineLightRigManager::instance().extraRimSceneEpoch()))
    {
        // A new-scene edit must not join the previous scene's pending batch.
        pendingExtraRimHistory.erase(found);
        found = pendingExtraRimHistory.end();
    }
    if ((!input || input->pageKey() == "light_group/light_rim_tab") && found == pendingExtraRimHistory.end())
    {
        recordExtraRimSnapshot(shell, page, before, after, label);
        return;
    }
    std::shared_ptr<PendingExtraRimHistory> pending;
    if (found != pendingExtraRimHistory.end()) pending = found->second;
    else
    {
        pending = std::make_shared<PendingExtraRimHistory>();
        pending->shell = shell->getHandle(); pending->page = page->getHandle();
        pending->sceneEpoch = ALCineLightRigManager::instance().extraRimSceneEpoch();
        pendingExtraRimHistory[shell] = pending;
        doOnIdleOneTime([pending, key = shell]()
        {
            const auto found = pendingExtraRimHistory.find(key);
            if (found != pendingExtraRimHistory.end() && found->second == pending)
                pendingExtraRimHistory.erase(found);
            auto* shell = dynamic_cast<ALKitShell*>(pending->shell.get());
            auto* page = pending->page.get();
            if (!shell || !page)
            {
                LLFloater* director = LLFloaterReg::getInstance("director");
                shell = director ? director->findChild<ALKitShell>("director_shell") : nullptr;
                page = shell ? shell->ensurePageBuilt("light_group/light_rim_tab") : nullptr;
            }
            // A scene/reset boundary cancels deferred provenance. Unrelated
            // history edits must never discard this extra-rim undo step.
            if (!shell || !page || ALCineLightRigManager::instance().extraRimSceneEpoch() != pending->sceneEpoch) return;
            for (auto& edit : pending->edits)
            {
                resolveExtraRimSnapshot(edit.before);
                resolveExtraRimSnapshot(edit.after);
                recordExtraRimSnapshot(shell, page, edit.before, edit.after, edit.label);
            }
        });
    }
    pending->edits.push_back({before, after, label});
    for (const LLSD* block : { &before, &after })
        for (const auto& item : llsd::inArray((*block)["items"]))
            pending->actors.push_back(ALCineLightRigManager::instance().retainBorrowActor(item["actor"].asUUID()));
}

namespace
{
std::set<ALExtraRimUI*> extraRimEditors;
}
void ALExtraRimUI::finishPendingTrims()
{
    for (auto* editor : extraRimEditors) editor->finishTrim(true);
}

ALExtraRimUI::~ALExtraRimUI()
{
    finishTrim(true);
    extraRimEditors.erase(this);
    if (auto* menu = mMenu.get()) { menu->hide(); menu->die(); }
}
void ALExtraRimUI::attach(LLPanel* root)
{
    extraRimEditors.insert(this);
    mRoot = root;
    mList = root->findChild<LLScrollListCtrl>("cine_extra_rim_list");
    mSource = root->findChild<LLComboBox>("cine_extra_rim_source");
    mTrim = root->findChild<LLSliderCtrl>("cine_extra_rim_trim");
    mStatus = root->findChild<LLTextBox>("cine_extra_rim_status");
    mAdd = root->findChild<LLButton>("cine_extra_rim_add");
    mRemove = root->findChild<LLButton>("cine_extra_rim_remove");
    if (!mList || !mSource || !mTrim || !mStatus || !mAdd || !mRemove)
    {
        LL_WARNS_ONCE("CineRig") << "Extra rims card is missing required widgets" << LL_ENDL;
        return;
    }
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&) { finishTrim(true); syncSelection(); });
    mList->setRightMouseDownCallback([this](LLUICtrl*, S32 x, S32 y, MASK) { showMenu(x, y); });
    mSource->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        finishTrim(true);
        auto& rigs = ALCineLightRigManager::instance();
        const LLSD before = rigs.captureExtraRims();
        rigs.setExtraRimSource(mSource->getValue().asInteger());
        recordEdit(ALKitShell::find(mRoot), before, "Extra rims direction");
    });
    mTrim->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        auto& rigs = ALCineLightRigManager::instance();
        if (mTrimBefore.isUndefined() || mTrimSceneEpoch != rigs.extraRimSceneEpoch())
        {
            mTrimBefore = rigs.captureExtraRims();
            mTrimActor = selectedActor();
            mTrimPick = rigs.retainBorrowActor(mTrimActor);
            mTrimSceneEpoch = rigs.extraRimSceneEpoch();
        }
        rigs.setExtraRimTrim(selectedActor(), (F32)mTrim->getValue().asReal() * .01f);
    });
    // LLSlider synthesises this callback once when a wheel gesture settles.
    mTrim->setSliderMouseUpCallback([this](LLUICtrl*, const LLSD&) { finishTrim(); });
    mRemove->setCommitCallback([this](LLUICtrl*, const LLSD&) { removeSelected(); });
    mAdd->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        finishTrim(true);
        auto* shell = ALKitShell::find(mRoot);
        if (!shell || shell->castSelection().empty()) return;
        auto& rigs = ALCineLightRigManager::instance();
        const LLUUID picked = shell->castSelection().front();
        const auto retained = rigs.retainBorrowActor(picked);
        const LLUUID actor = rigs.resolveBorrowActor(picked, true);
        const LLSD before = rigs.captureExtraRims();
        rigs.addExtraRim(actor);
        recordEdit(shell, before, "Add rim light");
    });
    syncSelection();
}
LLUUID ALExtraRimUI::selectedActor() const
{
    const auto* row = mList ? mList->getFirstSelected() : nullptr;
    return row ? row->getValue().asUUID() : LLUUID::null;
}
void ALExtraRimUI::finishTrim(bool force)
{
    if (mTrimBefore.isUndefined() || (!force && mTrim &&
        (mTrim->isMouseHeldDown() || mTrim->isWheelSettlePending()))) return;
    auto& rigs = ALCineLightRigManager::instance();
    if (mTrimSceneEpoch != rigs.extraRimSceneEpoch())
    {
        mTrimBefore = LLSD();
        mTrimPick.reset();
        return;
    }
    LLSD before = rigs.captureExtraRims();
    const LLUUID actor = rigs.resolveBorrowActor(mTrimActor, true);
    // Rebase unrelated changes made while the gesture was pending. The undo
    // delta owns only this actor's trim, never another command's add/source.
    for (const auto& original : llsd::inArray(mTrimBefore["items"]))
        if (original["actor"].asUUID() == mTrimActor)
            for (S32 i = 0; i < before["items"].size(); ++i)
                if (before["items"][i]["actor"].asUUID() == actor)
                    before["items"][i]["trim"] = original["trim"];
    recordEdit(ALKitShell::find(mRoot), before, "Extra rim strength");
    mTrimBefore = LLSD();
    mTrimPick.reset();
}
void ALExtraRimUI::syncSelection()
{
    const LLUUID id = selectedActor();
    const auto& rigs = ALCineLightRigManager::instance();
    const bool selected = rigs.hasExtraRim(id);
    setEnabledIfChanged(mTrim, selected);
    setEnabledIfChanged(mRemove, selected);
    if (selected)
        for (const auto& row : rigs.extraRims())
            if (row.mActor == id) setValueIfChanged(mTrim, row.mTrim * 100.f, true);
}
void ALExtraRimUI::removeSelected()
{
    finishTrim(true);
    auto& rigs = ALCineLightRigManager::instance();
    const LLSD before = rigs.captureExtraRims();
    rigs.removeExtraRim(selectedActor());
    recordEdit(ALKitShell::find(mRoot), before, "Remove rim light");
}
void ALExtraRimUI::showMenu(S32 x, S32 y)
{
    finishTrim(true);
    auto* row = mList->hitItem(x, y);
    if (!row) return;
    mList->selectNthItem(mList->getItemIndex(row));
    syncSelection();
    if (auto* old = mMenu.get()) { old->hide(); old->die(); }
    LLContextMenu::Params params; params.name = "extra_rim_menu";
    auto* menu = LLUICtrlFactory::create<LLContextMenu>(params);
    mMenu = menu->getHandle(); gMenuHolder->addChild(menu);
    LLMenuItemCallGL::Params remove; remove.name = "remove"; remove.label = "Remove rim light";
    const LLUUID picked = selectedActor();
    remove.on_click.function = [this, picked, retained = ALCineLightRigManager::instance().retainBorrowActor(picked)](LLUICtrl*, const LLSD&)
    {
        finishPendingTrims();
        auto& rigs = ALCineLightRigManager::instance();
        const LLUUID actor = rigs.resolveBorrowActor(picked, true);
        const LLSD before = rigs.captureExtraRims();
        rigs.removeExtraRim(actor);
        recordEdit(ALKitShell::find(mRoot), before, "Remove rim light");
    };
    menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(remove));
    menu->buildDrawLabels(); menu->updateParent(LLMenuGL::sMenuContainer); menu->show(x, y);
    LLMenuGL::showPopup(mList, menu, x, y);
}
void ALExtraRimUI::refresh()
{
    if (!mList || !isWidgetOnScreen(mRoot)) return;
    if (mTrim && (mTrim->isMouseHeldDown() || mTrim->isWheelSettlePending())) return;
    auto& rigs = ALCineLightRigManager::instance();
    const char* summary = rigs.extraRimSummary();
    if (mSummary != summary)
    {
        setTextIfChanged(mStatus, summary);
        mSummary = summary;
    }
    auto* shell = ALKitShell::find(mRoot);
    const LLUUID cast_actor = shell && !shell->castSelection().empty() ? shell->castSelection().front() : LLUUID::null;
    if (mRevision == rigs.extraRimRevision() && mCastActor == cast_actor &&
        mConfigurationRevision == rigs.configurationRevision()) return;
    mCastActor = cast_actor;
    mConfigurationRevision = rigs.configurationRevision();
    if (mRevision != rigs.extraRimRevision())
    {
        mRevision = rigs.extraRimRevision();
        const LLUUID selected = selectedActor();
        mList->deleteAllItems(); // ui-draw-lint: allow(raw-write) at most four rows, only on extraRimRevision change, visible at 10 Hz
        for (const auto& extra : rigs.extraRims())
        {
            LLSD row; row["value"] = extra.mActor;
            const auto* actor = LLDirectorCast::instance().getStoredMember(extra.mActor);
            row["columns"][0]["column"] = "cine_extra_rim_actor_col";
            row["columns"][0]["value"] = actor && !actor->mLastName.empty() ? actor->mLastName : "Actor";
            row["columns"][1]["column"] = "cine_extra_rim_strength_col";
            row["columns"][1]["value"] = llformat("%.0f%%", extra.mTrim * 100.f);
            row["columns"][2]["column"] = "cine_extra_rim_status_col";
            row["columns"][2]["value"] = extra.mStatus;
            mList->addElement(row, ADD_BOTTOM); // ui-draw-lint: allow(raw-write) same bounded revision-only list refresh
        }
        mList->setSelectedByValue(selected, true);
        syncSelection();
    }
    setValueIfChanged(mSource, rigs.extraRimSource());
    std::string why = "Select an actor in Cast";
    const bool can_add = shell && !shell->castSelection().empty() &&
        rigs.canAddExtraRim(shell->castSelection().front(), &why);
    setEnabledIfChanged(mAdd, can_add);
    setToolTipIfChanged(mAdd, can_add ? "Add one rim to the selected cast actor" : why);
}
