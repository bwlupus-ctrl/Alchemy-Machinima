/**
 * @file altoolvirtuallightplace.h
 * @brief One-shot in-world click tool: place a virtual light, or aim one at a spot or object.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Modelled on ALToolFocusPick: armed by one button, the next left click resolves a world point
 * (pickImmediate, mPosGlobal, never mIntersection), Esc / right click / losing the tool cancels.
 * Nothing here talks to the simulator.
 */
#ifndef AL_ALTOOLVIRTUALLIGHTPLACE_H
#define AL_ALTOOLVIRTUALLIGHTPLACE_H

#include "lltool.h"
#include "llsingleton.h"
#include "lluuid.h"

#include <functional>
#include <string>

class ALToolVirtualLightPlace final : public LLTool, public LLSingleton<ALToolVirtualLightPlace>
{
    LLSINGLETON(ALToolVirtualLightPlace);

public:
    // `done(ok, text)` fires exactly once per arm: ok on a committed click, !ok (text = why) on
    // cancel or a refusal that ends the arm. A retryable refusal leaves the tool armed.
    using DoneCallback = std::function<void(bool ok, const std::string& text)>;

    bool armPlace(DoneCallback done);
    bool armAim(const LLUUID& light_key, DoneCallback done);
    void cancel();
    bool isArmed() const { return mMode != Mode::NONE; }

    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;
    void handleSelect() override;
    void handleDeselect() override;

private:
    enum class Mode { NONE, PLACE, AIM };
    void disarm(bool notify_cancel);
    void start(Mode mode, DoneCallback done);

    Mode         mMode = Mode::NONE;
    LLUUID       mLightKey;
    DoneCallback mDone;
};

#endif // AL_ALTOOLVIRTUALLIGHTPLACE_H
