#ifndef AL_KEYFRAME_INPUT_GATE_H
#define AL_KEYFRAME_INPUT_GATE_H
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
// STATELESS clean-input rules (round 4). While ALCleanFrame is active:
//  1. DOWN of a denied action: dropped (native global route AND scan route).
//  2. LEVEL of a denied action: dropped.
//  3. UP: always delivered to stock, except the UP of a denied action whose UP itself has a
//     side effect (opens chat/UI, or would move the camera): that UP is dropped.
//  4. No per-key records, skip sets, flush, or ownership: nothing survives the lease, so
//     after it ends input is exactly stock.
class ALKeyframeInputGate
{
public:
    enum class Phase { Down, Level, Up };
    static bool seatedMoveAgent(bool grabbed, bool sitCamera, bool running, bool alwaysRun)
    { return grabbed || sitCamera || (running && !alwaysRun); }
    static bool denied(std::string_view action, bool cameraOnlyMove = false)
    {
        for (auto name : {"start_chat", "start_gesture", "toggle_viewfinder", "toggle_colour_chart",
                          "toggle_actor_key", "teleport_to", "walk_to"})
            if (action == name) return true;
        for (auto prefix : {"pan_", "spin_", "roll_", "edit_avatar_"})
            if (action.starts_with(prefix)) return true;
        // WP3 classifies move_* wrappers by the branch they actually execute.
        return cameraOnlyMove;
    }
    // A denied action whose UP has a side effect of its own. start_gesture/start_chat open
    // chat; a *_sitting wrapper resolved to its camera branch (agent branches are reported
    // as "agent_seated_control", never denied) orbits the camera on UP when double-tap
    // mode matches. Every other denied action's stock UP is a no-op (verified per action).
    static bool upHasSideEffect(std::string_view action)
    {
        return action == "start_chat" || action == "start_gesture" || action.ends_with("_sitting");
    }
    // The branch stock scanKey takes for one key edge set: DOWN (not a repeat), else UP, else LEVEL.
    static Phase phaseOf(bool down, bool up, bool repeat)
    { return (down && !repeat) ? Phase::Down : up ? Phase::Up : Phase::Level; }
    bool allow(std::string_view action, Phase phase, bool clean, bool cameraOnlyMove = false) const
    {
        if (!clean || !denied(action, cameraOnlyMove)) return true;
        return phase == Phase::Up && !upHasSideEffect(action);
    }
    // no data members: stateless by construction
};
// Mouse ownership is the only clean-input state: a button pressed in Play keeps its own UP
// even after the lease ends. Keys, gestures and bindings have no state at all.
class ALKeyframeMouseGate
{
public:
    bool mouseDown(unsigned button, bool clean)
    {
        if (clean) mMouse.insert(button);
        return clean || mMouse.count(button) != 0;
    }
    bool mouseUp(unsigned button) { return mMouse.erase(button) != 0; }
private:
    std::set<unsigned> mMouse;
};
#endif
