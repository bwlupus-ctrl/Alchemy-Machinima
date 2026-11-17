#ifndef AL_KEYFRAME_INPUT_GATE_H
#define AL_KEYFRAME_INPUT_GATE_H
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
// Call releaseKey exactly once BEFORE either global or ordinary stock UP lookup.
// A true return suppresses both stock UP routes for that physical key (Rev 5).
class ALKeyframeInputGate
{
public:
    enum class Phase { Down, Level };
    using Release = std::function<void()>;
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
    bool allow(std::uint32_t key, std::string_view action, Phase phase, bool clean,
               bool cameraOnlyMove = false)
    {
        if (mSwallowed.count(key))
        {
            // Only an action already started by this physical press may continue.
            // A modifier/mode change must not create an action with no owned UP.
            auto found=mStarted.find(key);
            if (phase==Phase::Level && found!=mStarted.end())
                for (const auto& entry:found->second) if(entry.name==action) return true;
            return false;
        }
        if (phase == Phase::Down && clean && denied(action, cameraOnlyMove))
        {
            mSwallowed.insert(key);
            return false;
        }
        return true;
    }
    // Record only after a DOWN has actually started the action. Deduplicate repeats.
    void started(std::uint32_t key, std::string_view action, Release up, bool clean)
    {
        if (!clean) return;
        auto& entries = mStarted[key];
        for (const auto& entry : entries) if (entry.name == action) return;
        entries.push_back({std::string(action), std::move(up)});
    }
    bool releaseKey(std::uint32_t key)
    {
        auto found = mStarted.find(key);
        bool owned = found != mStarted.end();
        if (owned)
        {
            auto actions = std::move(found->second);
            mStarted.erase(found); // callbacks can end Play or reenter input
            for (auto& action : actions) action.up();
        }
        return mSwallowed.erase(key) != 0 || owned;
    }
    bool mouseDown(unsigned button, bool clean)
    {
        if (clean) mMouse.insert(button);
        return clean || mMouse.count(button) != 0;
    }
    bool mouseUp(unsigned button) { return mMouse.erase(button) != 0; }
private:
    struct Action { std::string name; Release up; };
    std::set<std::uint32_t> mSwallowed;
    std::map<std::uint32_t, std::vector<Action>> mStarted;
    std::set<unsigned> mMouse;
};
#endif
