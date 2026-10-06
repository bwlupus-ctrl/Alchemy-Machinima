/**
 * @file llsliderwheel.cpp
 * @brief [SliderWheel] Viewer-wide slider mouse wheel: guard, settle registry, lifetime helpers.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Design: doc/SLIDER_WHEEL_GLOBAL_DESIGN.md (v7), sections 1, 4.2, 4.2a, 4.2b.
 *
 * Nothing here stores a raw LLSlider*, signal or callback owner: the registry holds
 * LLHandle<LLSlider> + generation + deadline only, and a pointer exists only for the duration
 * of one handle.get() use. Destruction paths only CANCEL; nothing fires from a destructor or a
 * visibility change.
 */

#include "linden_common.h"

#include "llsliderwheel.h"

#include "llapp.h"
#include "lleventtimer.h"
#include "llcontrol.h"
#include "llfloater.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "lltimer.h"
#include "llui.h"

namespace LLSliderWheel
{
namespace
{
typedef GuardState<LLHandle<LLView>, LLView> guard_t;
typedef SettleTable<LLHandle<LLSlider> >     table_t;

guard_t sGuard;
table_t sTable;
bool    sQuitting = false;

class SettleTimer;
SettleTimer* sTimer = nullptr;

void ensureTimer();

bool quitSeen()
{
    return sQuitting || LLFloater::isQuitRequested() || LLApp::isExiting();
}

// [SliderWheel] The shared settle timer. Exists only while the registry is non-empty.
class SettleTimer : public LLEventTimer
{
public:
    SettleTimer() : LLEventTimer(TICK_SEC) {}
    ~SettleTimer() override
    {
        if (sTimer == this)
        {
            sTimer = nullptr;
        }
    }

    bool tick() override
    {
        // 1. Quit: cancel everything, fire nothing, stop.
        if (quitSeen())
        {
            for (table_t::Entry& e : sTable.takeAll())
            {
                if (LLSlider* s = e.handle.get())
                {
                    s->cancelWheelSettle();
                }
            }
            sTimer = nullptr;
            return true;
        }

        // 2. Work on a swapped-out copy; every handle is re-resolved immediately before use.
        std::vector<table_t::Entry> work = sTable.takeAll();
        const F64 t = now();
        for (table_t::Entry& e : work)
        {
            // [SliderWheel] review P1-1: decisions use alive() (the slider AND every ancestor not
            // die()d), not the bare handle; a die()d intermediate ancestor cancels the entry.
            LLSlider* raw = e.handle.get();
            LLSlider* s = alive(e.handle);
            if (raw && !s)
            {
                raw->cancelWheelSettle();
            }
            if (quitSeen())
            {
                // review P1-3: a callback earlier in this tick began the quit: cancel, never fire
                if (s)
                {
                    s->cancelWheelSettle();
                }
                continue;
            }
            const TickAction action = tickDecision(
                s != nullptr,
                s && s->getWheelSettleGen() == e.gen,
                s && s->isWheelSettlePending(),
                s && s->hasMouseCapture(),
                t, e.deadline);
            switch (action)
            {
            case TickAction::Drop:
                break;
            case TickAction::Defer:
                e.deadline = t + SETTLE_SEC;
                sTable.insertMerge(e);
                break;
            case TickAction::Wait:
                sTable.insertMerge(e);
                break;
            case TickAction::Fire:
                // fresh alive() resolve right before the fire (review P1-1)
                if (LLSlider* fire = alive(e.handle))
                {
                    fire->fireWheelSettle(e.gen);
                }
                else if (LLSlider* dead = e.handle.get())
                {
                    dead->cancelWheelSettle();
                }
                break;
            }
        }

        if (sTable.empty())
        {
            sTimer = nullptr;
            return true; // LLEventTimer::updateClass deletes this
        }
        return false;
    }
};

void ensureTimer()
{
    if (!sTimer && !sQuitting)
    {
        sTimer = new SettleTimer();
    }
}
} // namespace

// ---- guard ----------------------------------------------------------------------------------

F64 now()
{
    return LLTimer::getTotalSeconds();
}

bool switchOn()
{
    static LLCachedControl<bool> sSwitch(*LLUI::getInstance()->mSettingGroups["config"], "UISliderWheelAdjust", true);
    return sSwitch;
}

void noteScroll(LLView* owner)
{
    if (owner)
    {
        sGuard.note(owner->getHandle(), now());
    }
}

void keepAlive()
{
    sGuard.keepAlive(now());
}

bool guarded(const LLView* v)
{
    return v && sGuard.guarded(v, now());
}

LLSlider* sliderAt(LLView* container, S32 x, S32 y)
{
    if (!container)
    {
        return nullptr;
    }
    for (LLView* v = container->childFromPoint(x, y, true); v && v != container; v = v->getParent())
    {
        LLSlider* slider = dynamic_cast<LLSlider*>(v);
        if (!slider && dynamic_cast<LLSliderCtrl*>(v))
        {
            // wheel-forwarding slider ctrls (wheel_fine_div) route via their inner slider
            for (LLView* c : *v->getChildList())
            {
                if ((slider = dynamic_cast<LLSlider*>(c)) != nullptr) break;
            }
        }
        if (slider)
        {
            return slider;
        }
    }
    return nullptr;
}

// ---- settle ---------------------------------------------------------------------------------

bool isQuitting()
{
    return sQuitting;
}

void scheduleSettle(const LLHandle<LLSlider>& h, U32 gen, F64 deadline)
{
    if (sQuitting)
    {
        return;
    }
    sTable.upsert(h, gen, deadline);
    ensureTimer();
}

void settleForClose(LLHandle<LLFloater> root, U32 serial, bool app_quitting)
{
    LLFloater* r = root.get();
    if (!r)
    {
        return;
    }

    // 1. Collect (and remove) the entries of live sliders that are `r` or its descendants.
    //    Pure ancestry, no visibility involved, so a panel that never calls its base
    //    onVisibilityChange cannot block this. No foreign code runs in this step.
    std::vector<table_t::Entry> mine = sTable.extractIf([r](const table_t::Entry& e)
    {
        LLSlider* s = e.handle.get();
        return s && (static_cast<const LLView*>(s) == static_cast<const LLView*>(r) || s->hasAncestor(r));
    });
    if (mine.empty())
    {
        return;
    }

    // review P1-1: a slider under a die()d intermediate ancestor is selected above (it is still
    // a descendant) but must never fire: cancel it and drop it from the local list.
    {
        std::vector<table_t::Entry> live;
        for (table_t::Entry& e : mine)
        {
            if (alive(e.handle))
            {
                live.push_back(e);
            }
            else if (LLSlider* dead = e.handle.get())
            {
                dead->cancelWheelSettle();
            }
        }
        mine.swap(live);
        if (mine.empty())
        {
            return;
        }
    }

    // 2. Quit = cancel without firing.
    if (app_quitting || quitSeen())
    {
        for (table_t::Entry& e : mine)
        {
            if (LLSlider* s = e.handle.get())
            {
                s->cancelWheelSettle();
            }
        }
        return;
    }

    // 3. Fire. Before EACH fire: (a) review P1-3: re-check quit (an earlier mouse-up callback may
    //    have started the exit or called cancelAllForQuit) and cancel every remaining local
    //    entry; (b) re-check the root: deleted, dead, or superseded by a nested close / reopen
    //    (serial changed) => stop and give the unfired entries back to the tick; (c) resolve the
    //    slider with alive() (review P1-1), cancelling it if its ancestry died meanwhile.
    for (size_t i = 0; i < mine.size(); ++i)
    {
        if (app_quitting || quitSeen())
        {
            for (size_t j = i; j < mine.size(); ++j)
            {
                if (LLSlider* s = mine[j].handle.get())
                {
                    s->cancelWheelSettle();
                }
            }
            return;
        }
        LLFloater* f = root.get();
        if (!f || f->isDead() || f->getLifecycleSerial() != serial)
        {
            for (size_t j = i; j < mine.size(); ++j)
            {
                if (alive(mine[j].handle))
                {
                    sTable.insertMerge(mine[j]);
                }
                else if (LLSlider* dead = mine[j].handle.get())
                {
                    dead->cancelWheelSettle();
                }
            }
            if (!sTable.empty())
            {
                ensureTimer();
            }
            return;
        }
        if (LLSlider* s = alive(mine[i].handle)) // fresh resolve right before the fire
        {
            s->fireWheelSettle(mine[i].gen);
        }
        else if (LLSlider* dead = mine[i].handle.get())
        {
            dead->cancelWheelSettle();
        }
    }
}

void cancelAllForQuit()
{
    sQuitting = true;
    for (table_t::Entry& e : sTable.takeAll())
    {
        if (LLSlider* s = e.handle.get())
        {
            s->cancelWheelSettle();
        }
    }
    // The timer (if any) finds the registry empty on its next tick and deletes itself.
}
} // namespace LLSliderWheel
