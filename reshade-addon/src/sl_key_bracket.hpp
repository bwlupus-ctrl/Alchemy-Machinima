// [ActorKey] Lifetime-stable runtime registry. Tested with fake runtimes.
#pragma once
#include <Windows.h>
#include <cassert>
#include "sl_key_guard.hpp"
namespace SLKey {
inline thread_local bool t_in_bracket_lock = false;
inline void outsideBracket() { assert(!t_in_bracket_lock); }
// Bracket locks never call the viewer, logger, depth pin or allocator.
#define SL_BRACKET_LOCK AcquireSRWLockExclusive(&lock); t_in_bracket_lock=true; __try
#define SL_BRACKET_UNLOCK __finally { t_in_bracket_lock=false; ReleaseSRWLockExclusive(&lock); }
template<class Runtime> struct Brackets {
    struct Slot {
        Runtime* rt;
        void* device;
        uintptr_t hwnd;
        bool open, saved, engaged;
        uint32_t lastVerdict;
        uint64_t frame;
        uint32_t passes[2], begins[2], finishes[2]; // since-present, in-bracket
        uint32_t gui, reloadAt, reloadDuring, verdict, foreign;
    } slots[8]{};
    SRWLOCK lock = SRWLOCK_INIT;
    bool dead = false;
    uint64_t totals[3]{};
    Slot* find(Runtime* rt) { for (auto& b:slots) if (b.rt==rt) return &b; return nullptr; } // lock held
    static void restore(Slot& b) {
        if (b.open && !b.rt->get_effects_state()) b.rt->set_effects_state(b.saved);
        b.open=false;
    }
    bool init(Runtime* rt, void* device, uintptr_t hwnd) {
        bool ok=false;
        SL_BRACKET_LOCK {
            Slot* b=find(rt);
            if (!b) for (auto& entry:slots) if (!entry.rt) { b=&entry; break; }
            if (b) { restore(*b); *b={}; b->rt=rt; b->device=device; b->hwnd=hwnd; ok=true; }
        } SL_BRACKET_UNLOCK
        return ok;
    }
    void destroy(Runtime* rt) {
        SL_BRACKET_LOCK { Slot* b=find(rt); if (b) { restore(*b); *b={}; } } SL_BRACKET_UNLOCK
    }
    // 0/multiple matches must not mutate any runtime.
    bool open(void* device, uintptr_t hwnd, uint64_t frame, uint32_t verdict, uint32_t foreign, uint32_t gui, Runtime** selectedRt=nullptr, bool* firstOpen=nullptr) {
        bool ok=false;
        SL_BRACKET_LOCK {
            Slot* selected=nullptr; unsigned matches=0;
            for (auto& b:slots) if (b.rt && b.device==device && b.hwnd==hwnd) { selected=&b; ++matches; }
            if (matches==1 && !dead) {
                Slot& b=*selected;
                if(selectedRt) *selectedRt=b.rt;
                if (b.open) { restore(b); dead=true; }
                else {
                    if(firstOpen) *firstOpen=!b.engaged;
                    b.engaged=true;
                    b.frame=frame; b.verdict=verdict; b.foreign=foreign;
                    b.gui=(b.gui & (SLRESHADE_GUI_OVERLAY|SLRESHADE_GUI_RELOADED))|gui;
                    b.passes[1]=b.begins[1]=b.finishes[1]=0;
                    b.saved=b.rt->get_effects_state(); b.open=true; // BEFORE mutation, including a faulting call
                    if (b.saved) b.rt->set_effects_state(false);
                    ok=true;
                }
            }
        } SL_BRACKET_UNLOCK
        return ok;
    }
    bool finish(Runtime* rt, uint32_t now, SLReShadeKeyGuard& report, bool* verdictChanged=nullptr) {
        bool ready=false;
        SL_BRACKET_LOCK {
            Slot* b=find(rt);
            if (b) {
                if (b->open) {
                    restore(*b);
                    const uint32_t passes=(std::max)(b->passes[0],b->passes[1]);
                    const uint32_t calls=satAdd((std::max)(b->begins[0],b->begins[1]),(std::max)(b->finishes[0],b->finishes[1]));
                    uint32_t verdict=b->verdict;
                    if (verdict==SLRESHADE_GUARD_CLEAN && (passes||calls)) verdict=SLRESHADE_GUARD_CONTAMINATED;
                    uint32_t gui=b->gui;
                    if (b->reloadAt && uint32_t(now-b->reloadAt)<5000) gui|=SLRESHADE_GUI_RELOAD_SPLASH;
                    if(verdictChanged) *verdictChanged=b->lastVerdict!=verdict;
                    b->lastVerdict=verdict;
                    report={sizeof(report),verdict,b->frame,passes,gui,b->foreign,calls};
                    b->gui &= ~SLRESHADE_GUI_RELOADED;
                    ready=true;
                }
                b->passes[0]=b->begins[0]=b->finishes[0]=0;
            }
        } SL_BRACKET_UNLOCK
        return ready;
    }
    Runtime* idle(void* device, uintptr_t hwnd, bool& stale) {
        Runtime* closed=nullptr;
        SL_BRACKET_LOCK {
            Slot* selected=nullptr; unsigned matches=0;
            for(auto& b:slots) if(b.rt && b.device==device && b.hwnd==hwnd) { selected=&b; ++matches; }
            if(matches==1) {
                if(selected->open) { restore(*selected); dead=true; stale=true; }
                if(selected->engaged) { closed=selected->rt; selected->engaged=false; selected->lastVerdict=0; }
            }
        } SL_BRACKET_UNLOCK
        return closed;
    }
    void count(Runtime* rt, unsigned which) {
        SL_BRACKET_LOCK {
            Slot* b=find(rt);
            if (b) {
                ++totals[which];
                uint32_t* c=which==0 ? b->passes : which==1 ? b->begins : b->finishes;
                c[0]=satAdd(c[0],1); if (b->open) c[1]=satAdd(c[1],1);
            }
        } SL_BRACKET_UNLOCK
    }
    void reload(Runtime* rt, uint32_t now) {
        SL_BRACKET_LOCK { Slot* b=find(rt); if(b) {
            b->gui|=SLRESHADE_GUI_RELOADED; b->reloadAt=now;
            if(b->open) b->reloadDuring=satAdd(b->reloadDuring,1);
        } } SL_BRACKET_UNLOCK
    }
    void overlay(Runtime* rt, bool open) {
        SL_BRACKET_LOCK { Slot* b=find(rt); if(b) {
            if(open) b->gui|=SLRESHADE_GUI_OVERLAY; else b->gui&=~SLRESHADE_GUI_OVERLAY;
        } } SL_BRACKET_UNLOCK
    }
    bool isDead() { bool result; SL_BRACKET_LOCK { result=dead; } SL_BRACKET_UNLOCK return result; }
    void recover() {
        SL_BRACKET_LOCK {
            for(auto& b:slots) if(b.open) {
                __try { b.rt->set_effects_state(b.saved); }
                __except(EXCEPTION_EXECUTE_HANDLER) { /* Continue restoring other runtimes. */ }
                b.open=false;
            }
            dead=true;
        } SL_BRACKET_UNLOCK
    }
};
#undef SL_BRACKET_LOCK
#undef SL_BRACKET_UNLOCK
} // namespace SLKey
