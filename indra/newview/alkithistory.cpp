#include "llviewerprecompiledheaders.h"
#include "alkithistory.h"
#include "alkitbinding.h"
#include "alkitpage.h"
#include "alkitrow.h"
#include "alkitshell.h"
#include "alundosequence.h"
#include "llcontrol.h"
#include "lleventtimer.h"
#include "llfloaterdirector.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llnotificationsutil.h"
#include "lltimer.h"
#include "lluictrl.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"

#include <algorithm>
#include <unordered_set>

static S32 sKitHistorySuspend     = 0;
static F64 sKitHistoryKeyTime     = 0.;
static F64 sKitHistoryPointerTime = 0.;
static LLView* sKitInputSource = nullptr;
static bool sKitInputSourceSet = false;

namespace
{
std::string rowPage(LLView* view)
{
    if (ALKitRow* row = dynamic_cast<ALKitRow*>(view ? view->getParent() : nullptr))
        if (!row->meta().page_key.empty())
            return row->meta().page_key;
    LLView* root = view;
    for (; view; view = view->getParent())
        if (ALKitPage* page = dynamic_cast<ALKitPage*>(view))
            return page->pageKey();
    // Shared page controllers wrap their kit_page rather than sitting inside it.
    std::function<ALKitPage*(LLView*)> find_page = [&find_page](LLView* parent) -> ALKitPage*
    {
        for (LLView* child : *parent->getChildList())
        {
            if (ALKitPage* page = dynamic_cast<ALKitPage*>(child))
                return page;
            if (ALKitPage* page = find_page(child))
                return page;
        }
        return nullptr;
    };
    if (root)
        if (ALKitPage* page = find_page(root))
            return page->pageKey();
    return std::string();
}
}

ALKitHistory::ScopedSource::ScopedSource(LLView* source) :
    mPrevious(sKitInputSource), mPreviousSet(sKitInputSourceSet)
{ sKitInputSource = source; sKitInputSourceSet = true; }
ALKitHistory::ScopedSource::~ScopedSource()
{ sKitInputSource = mPrevious; sKitInputSourceSet = mPreviousSet; }

LLView* ALKitHistory::inputPage()
{
    LLView* view = sKitInputSource;
    if (!sKitInputSourceSet && !view)
        view = dynamic_cast<LLView*>(gFocusMgr.getMouseCapture());
    if (!sKitInputSourceSet && !view)
        view = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
    for (; view; view = view->getParent())
    {
        if (ALKitRow* row = dynamic_cast<ALKitRow*>(view))
            if (row->sourceRow())
                if (ALKitShell* shell = ALKitShell::find(row->sourceRow()))
                    return shell->findBuiltPage(row->sourceRow()->meta().page_key);
        if (dynamic_cast<ALKitPage*>(view))
            return view;
    }
    // Shared controllers contain their kit page; tool sessions retain that controller.
    if (sKitInputSourceSet && sKitInputSource)
        for (auto it = sKitInputSource->beginTreeBFS(); it != sKitInputSource->endTreeBFS(); ++it)
            if (dynamic_cast<ALKitPage*>(*it))
                return *it;
    return nullptr;
}

class ALKitHistory::GestureTimer final : public LLEventTimer
{
public:
    explicit GestureTimer(ALKitHistory* history) : LLEventTimer(.1f), mHistory(history) {}

    bool tick() override { return mHistory->tickGesture(); }

private:
    ALKitHistory* mHistory;
};

ALKitHistory::ALKitHistory()  = default;
ALKitHistory::~ALKitHistory() = default;

void ALKitHistory::setOwner(LLView* owner)
{ mOwner = owner ? owner->getHandle() : LLHandle<LLView>(); }

ALKitHistory::ScopedSuspend::ScopedSuspend()
{ ++sKitHistorySuspend; }
ALKitHistory::ScopedSuspend::~ScopedSuspend()
{ --sKitHistorySuspend; }
bool ALKitHistory::suspended()
{ return sKitHistorySuspend > 0; }

void ALKitHistory::beginGesture(EOrigin o, const std::string& p, const std::string& label)
{
    if (mOpen && mOrigin == o && mPage == p && (o == WHEEL || o == KEY))
    {
        if (!label.empty() && mOpen->label.empty())
        {
            mOpen->label = label;
        }
        mCloseAt = LLTimer::getTotalSeconds() + .5;
        return;
    }
    finishGesture();
    mOrigin                 = o;
    mPage                   = p;
    mGestureLabel           = label;
    mOpen                   = std::make_unique<Step>();
    mOpen->page             = p;
    mOpen->label            = label;
    mOpen->scene_generation = mGeneration;
    mOpen->context          = currentContext(p);
    mCloseAt                = LLTimer::getTotalSeconds() + ((o == WHEEL || o == KEY) ? .5 : .35);
    if (!mTimer)
        mTimer = std::make_unique<GestureTimer>(this);
}
void ALKitHistory::noteKey()
{ sKitHistoryKeyTime = LLTimer::getTotalSeconds(); }

void ALKitHistory::ensureGesture(const std::string& page, const std::string& label)
{
    if (!mOpen || mOpen->page != page || mOrigin == KEY)
        beginGesture(KEY, page, label);
}

void ALKitHistory::notePointer(LLView* hit_view)
{
    if (ALKitShell::find(hit_view))
        sKitHistoryPointerTime = LLTimer::getTotalSeconds();
}

void ALKitHistory::watchSettings(const std::string& page, const std::vector<std::string>& names)
{
    for (const std::string& name : names)
    {
        auto& connections = mWatches[name];
        connections.emplace_back(ALKitSettingWatch::instance().watch(name,
                                                                     [this, page, name](const LLSD& value, const LLSD& old)
                                                                     { captureSetting(page, name, value, old); }));
    }
}

void ALKitHistory::recordWidget(LLUICtrl* c, const LLSD& oldv, const LLSD& newv, const std::string& label)
{
    if (suspended() || mReplaying || !c || oldv == newv)
        return;
    const std::string page = rowPage(c);
    if (page.empty())
        return;
    const bool standalone = !mOpen || mOpen->page != page;
    if (standalone)
        beginGesture(KEY, page, label);
    Entry e;
    e.widget = true;
    e.key    = c->getName();
    e.ctrl   = c->getHandle();
    e.oldv   = oldv;
    e.newv   = newv;
    addEntry(std::move(e), label);
}

void ALKitHistory::recordCustom(LLView* owner, const std::string& key, const std::string& label,
                                 std::function<bool(bool, std::string&)> fn)
{
    if (suspended() || mReplaying)
        return;
    const std::string page = rowPage(owner);
    if (page.empty())
        return;
    // A list command is one complete action, including a keyboard Add with no open gesture.
    finishGesture();
    beginGesture(KEY, page, label);
    mOpen->custom.push_back({ key, std::move(fn) });
    mOpen->sequence = alNextUndoSequence();
    if (mOpen->label.empty())
        mOpen->label = label;
    finishGesture();
}

void ALKitHistory::barrier(const std::string& reason)
{
    mOpen.reset();
    mTimer.reset();
    mUndo.clear();
    mRedo.clear();
    ++mGeneration;
    ++mRevision;
    LL_INFOS("DirectorKit") << "History barrier: " << reason << LL_ENDL;
}
void ALKitHistory::globalBarrier(const std::string& reason)
{
    if (LLFloaterDirector* director = LLFloaterReg::findTypedInstance<LLFloaterDirector>("director"))
    {
        if (ALKitHistory* history = director->kitHistory())
            history->barrier(reason);
    }
}

void ALKitHistory::captureSetting(const std::string& page, const std::string& name, const LLSD& value, const LLSD& old)
{
    if (suspended() || mReplaying || value == old)
        return;
    LLView*    owner       = mOwner.get();
    ALKitPage* source = dynamic_cast<ALKitPage*>(inputPage());
    if (page != "chrome" && (!source || source->pageKey() != page || ALKitShell::find(source) != owner))
        return;
    LLView*    focus       = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
    const F64  now         = LLTimer::getTotalSeconds();
    const bool owner_focus = owner && focus && (focus == owner || focus->hasAncestor(owner));
    const bool keyboard    = (sKitInputSourceSet && source) ||
        (owner_focus && now - llmax(sKitHistoryKeyTime, sKitHistoryPointerTime) <= .5);
    const bool pointer     = mOpen && (mOrigin == MOUSE || mOrigin == WHEEL);
    if (!pointer && !keyboard)
        return;
    if (!mOpen || (page != "chrome" && mOpen->page != page))
    {
        beginGesture(KEY, page);
    }
    else if (mOrigin == KEY && mOpen->page == page)
    {
        mCloseAt = now + .5;
    }
    if (page != mOpen->page && page != "chrome")
        return;
    Entry entry;
    entry.key  = name;
    entry.oldv = old;
    entry.newv = value;
    addEntry(std::move(entry), std::string(), page);
}

void ALKitHistory::addEntry(Entry entry, const std::string& supplied_label, const std::string& label_page)
{
    if (entry.oldv == entry.newv)
        return;
    if (!mOpen)
    {
        return; // Callers must establish the action's page before recording any entry.
    }
    if (ALKitShell* shell = dynamic_cast<ALKitShell*>(mOwner.get()))
        if (ALKitPage* page = shell->findBuiltPage(mOpen->page))
            page->historyEdited(false);
    for (Entry& existing : mOpen->entries)
    {
        if (existing.widget == entry.widget && existing.key == entry.key)
        {
            existing.newv = entry.newv;
            mOpen->sequence = alNextUndoSequence();
            ++mRevision;
            return;
        }
    }
    std::string label = supplied_label;
    if (label.empty())
    {
        label = entry.key;
        if (ALKitShell* shell = dynamic_cast<ALKitShell*>(mOwner.get()))
        {
            if (ALKitPage* page = shell->findBuiltPage(label_page))
            {
                if (ALKitRow* row = page->findRowBySetting(entry.key))
                {
                    label = shell->pagePathLabel(label_page) + " \xE2\x80\xBA " + row->meta().label;
                }
            }
        }
    }
    if (mOpen->label.empty())
        mOpen->label = label;
    mOpen->entries.push_back(std::move(entry));
    mOpen->sequence = alNextUndoSequence();
    ++mRevision;
}

void ALKitHistory::finishGesture()
{
    if (!mOpen)
        return;
    mOpen->entries.erase(
        std::remove_if(mOpen->entries.begin(), mOpen->entries.end(), [](const Entry& entry) { return entry.oldv == entry.newv; }),
        mOpen->entries.end());
    if (!mOpen->entries.empty() || !mOpen->custom.empty())
    {
        mUndo.push_back(std::move(*mOpen));
        if (mUndo.size() > MAX_STEPS)
            mUndo.erase(mUndo.begin());
        const std::string page = mUndo.back().page;
        mRedo.erase(std::remove_if(mRedo.begin(), mRedo.end(),
            [&page](const Step& step) { return step.page == page; }), mRedo.end());
        ++mRevision;
    }
    mOpen.reset();
}

bool ALKitHistory::tickGesture()
{
    if (!mOpen)
    {
        mTimer.release();
        return true;
    }
    const bool mouse_down = gViewerWindow && gViewerWindow->getLeftMouseDown();
    if (mouse_down && mOrigin == MOUSE)
    {
        mCloseAt = LLTimer::getTotalSeconds() + .35;
        return false;
    }
    if (LLTimer::getTotalSeconds() < mCloseAt)
        return false;
    finishGesture();
    mTimer.release();
    return true;
}

std::string ALKitHistory::currentContext(const std::string& page) const
{
    auto found = mContexts.find(page);
    return found == mContexts.end() ? std::string() : found->second();
}

bool ALKitHistory::replay(Step& step, bool redo)
{
    if (step.scene_generation != mGeneration)
        return false;
    const std::string context = currentContext(step.page);
    if (!step.context.empty() && context != step.context)
    {
        LLSD args;
        args["MESSAGE"] = "Switch back to " + step.context + " to undo this";
        LLNotificationsUtil::add("SystemMessageTip", args);
        return false;
    }
    ScopedSuspend guard;
    mReplaying = true;
    bool ok    = true;
    // A shared setting may have been edited on another page since this step.
    // Drop conflicting entries from the replay branch as well as this replay.
    bool skipped = false;
    std::unordered_set<std::string> conflicts;
    for (const Entry& entry : step.entries)
        if (!entry.widget)
        {
            LLControlVariable* control = gSavedSettings.getControl(entry.key).get();
            if (!control || control->get() != (redo ? entry.oldv : entry.newv))
                conflicts.insert(entry.key);
        }
    step.entries.erase(std::remove_if(step.entries.begin(), step.entries.end(),
        [&conflicts, &skipped](const Entry& entry)
        {
            bool conflict = conflicts.count(entry.key) != 0;
            if (entry.widget)
            {
                LLUICtrl* control = entry.ctrl.get();
                ALKitRow* row = control ? dynamic_cast<ALKitRow*>(control->getParent()) : nullptr;
                conflict = row && conflicts.count(row->meta().setting) != 0;
            }
            skipped = skipped || conflict;
            return conflict;
        }), step.entries.end());
    if (skipped)
        LLNotificationsUtil::add("SystemMessageTip",
            LLSD().with("MESSAGE", "Skipped settings changed elsewhere"));
    for (auto it = step.entries.rbegin(); it != step.entries.rend(); ++it)
    {
        Entry&      entry = *it;
        const LLSD& value = redo ? entry.newv : entry.oldv;
        if (entry.widget)
        {
            if (LLUICtrl* ctrl = entry.ctrl.get())
            {
                ctrl->setValue(value);
                ctrl->onCommit();
            }
        }
    }
    for (auto it = step.entries.rbegin(); it != step.entries.rend(); ++it)
    {
        Entry&      entry = *it;
        const LLSD& value = redo ? entry.newv : entry.oldv;
        if (!entry.widget)
            if (LLControlVariable* control = gSavedSettings.getControl(entry.key).get())
            {
                control->set(value);
            }
    }
    auto replay_custom = [&ok, redo](CustomEntry& entry)
    {
        std::string reason;
        if (!entry.replay(redo, reason))
        {
            LLSD args;
            args["MESSAGE"] = reason;
            LLNotificationsUtil::add("SystemMessageTip", args);
            ok = false;
        }
    };
    if (redo)
    {
        for (CustomEntry& entry : step.custom)
        {
            replay_custom(entry);
            if (!ok)
                break;
        }
    }
    else
    {
        for (auto it = step.custom.rbegin(); it != step.custom.rend(); ++it)
        {
            replay_custom(*it);
            if (!ok)
                break;
        }
    }
    mReplaying = false;
    return ok;
}

bool ALKitHistory::canUndo(const std::string& page) const
{
    return stepSequence(page, false) != 0;
}

U64 ALKitHistory::stepSequence(const std::string& page, bool redo) const
{
    if (!redo && mOpen && mOpen->page == page &&
        (!mOpen->custom.empty() || std::any_of(mOpen->entries.begin(), mOpen->entries.end(),
            [](const Entry& entry) { return entry.oldv != entry.newv; })))
        return mOpen->sequence;
    const auto& steps = redo ? mRedo : mUndo;
    auto it = std::find_if(steps.rbegin(), steps.rend(),
        [&page](const Step& step) { return step.page == page; });
    return it == steps.rend() ? 0 : it->sequence;
}

bool ALKitHistory::canRedo(const std::string& page) const
{
    return std::any_of(mRedo.begin(), mRedo.end(), [&page](const Step& step) { return step.page == page; });
}

void ALKitHistory::discardRedo(const std::string& page)
{
    const auto count = mRedo.size();
    mRedo.erase(std::remove_if(mRedo.begin(), mRedo.end(),
        [&page](const Step& step) { return step.page == page; }), mRedo.end());
    if (count != mRedo.size())
        ++mRevision;
}

void ALKitHistory::undo(const std::string& page)
{
    finishGesture();
    mTimer.reset();
    auto it = std::find_if(mUndo.rbegin(), mUndo.rend(), [&page](const Step& step) { return step.page == page; });
    if (it == mUndo.rend())
        return;
    if (!replay(*it, false))
        return;
    Step s = std::move(*it);
    mUndo.erase(std::next(it).base());
    s.sequence = alNextUndoSequence();
    if (!s.entries.empty() || !s.custom.empty())
        mRedo.push_back(std::move(s));
    ++mRevision;
}
void ALKitHistory::redo(const std::string& page)
{
    finishGesture();
    mTimer.reset();
    auto it = std::find_if(mRedo.rbegin(), mRedo.rend(), [&page](const Step& step) { return step.page == page; });
    if (it == mRedo.rend())
        return;
    if (!replay(*it, true))
        return;
    Step s = std::move(*it);
    mRedo.erase(std::next(it).base());
    s.sequence = alNextUndoSequence();
    if (!s.entries.empty() || !s.custom.empty())
        mUndo.push_back(std::move(s));
    ++mRevision;
}

void ALKitHistory::registerContext(const std::string& key, std::function<std::string()> context)
{ mContexts[key] = std::move(context); }
