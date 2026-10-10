/**
 * Login-screen UI draw performance harness.
 */
#ifndef AL_UI_PERF_SELF_TEST_H
#define AL_UI_PERF_SELF_TEST_H

#include "stdtypes.h"

class ALUIPerfSelfTest
{
public:
    // Called once when startup enters STATE_LOGIN_WAIT. Empty setting: one
    // cached-control check and no runner allocation.
    static void startIfRequested();

    // Called once per login-startup frame, after the idle callback list has
    // finished iterating. May open/close floaters and switch tabs.
    static void tick();

    // Render hooks. They are inert unless a requested test is active.
    static void recordUIDraw(F64 milliseconds);
    static void recordViewCounts(U32 drawn, U32 culled,
                                 U32 find_calls, U32 find_visited);

    // Called after the login-screen UI has been flushed and before its back
    // buffer is swapped.  When a layout-audit shot is pending this copies only
    // the requested floater rectangle; PNG encoding happens later in tick().
    static void captureAfterRender();
    static bool layoutAuditActive();

    // A self-test session must not serialize UI/open-state side effects.
    static bool suppressSettingsSave();
    static bool isActive();
};

#endif
