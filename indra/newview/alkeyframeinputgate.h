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
        if (clean && denied(action, cameraOnlyMove))
        {
            if (phase == Phase::Down)
            {
                mSwallowed.insert(key);
                return false;
            }
            // A denied action can never START on a LEVEL either (modifier change while
            // held); only this (key, action) pair loses its stock UP, not the whole key.
            if (!mSkip[key].count(action)) mSkip[key].emplace(action);
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
    // A LEVEL action that was not started by a recorded DOWN (a modifier changed while the
    // key was held, a mixed global/ordinary binding) acquires release ownership the first
    // time it executes, so the physical UP reaches it exactly once. makeUp is only called
    // when a record is actually created (no per-frame allocation).
    // stockOwned: a stock owner (pre-Play hot gaze) already delivers this UP; never duplicate.
    template <class MakeUp>
    void levelStarted(std::uint32_t key, std::string_view action, MakeUp makeUp, bool clean,
                      bool stockOwned = false)
    {
        if (!clean || stockOwned) return;
        auto found = mStarted.find(key);
        if (found != mStarted.end())
            for (const auto& entry : found->second) if (entry.name == action) return;
        mStarted[key].push_back({std::string(action), makeUp()});
    }
    bool releaseKey(std::uint32_t key)
    {
        bool owned = deliver(key, false);
        return mSwallowed.erase(key) != 0 || owned;
    }
    // Binding route: deliver the recorded UPs and remember ONLY those (key, action) pairs so
    // the stock UP lookup skips them (skipStockUp). Returns true only for a denied-key
    // press (Rev 5 persistence); any other action keeps its stock UP exactly once.
    bool releaseBindings(std::uint32_t key)
    {
        deliver(key, true);
        return mSwallowed.erase(key) != 0;
    }
    bool skipStockUp(std::uint32_t key, std::string_view action) const
    {
        auto found = mSkip.find(key);
        return found != mSkip.end() && found->second.count(action) != 0;
    }
    void endStockUp(std::uint32_t key) { mSkip.erase(key); }
    // Lease end: deliver every outstanding recorded UP exactly once and clear the records,
    // so the input system is stock again. Denied-key persistence is kept.
    void flush()
    {
        auto all = std::move(mStarted);
        mStarted.clear();
        for (auto& entry : all)
            for (auto& action : entry.second) action.up();
    }
    bool mouseDown(unsigned button, bool clean)
    {
        if (clean) mMouse.insert(button);
        return clean || mMouse.count(button) != 0;
    }
    bool mouseUp(unsigned button) { return mMouse.erase(button) != 0; }
private:
    bool deliver(std::uint32_t key, bool markSkip)
    {
        auto found = mStarted.find(key);
        if (found == mStarted.end()) return false;
        auto actions = std::move(found->second);
        mStarted.erase(found); // callbacks can end Play or reenter input
        if (markSkip) for (const auto& action : actions) mSkip[key].insert(action.name);
        for (auto& action : actions) action.up();
        return true;
    }
    struct Action { std::string name; Release up; };
    std::set<std::uint32_t> mSwallowed;
    std::map<std::uint32_t, std::vector<Action>> mStarted;
    std::set<unsigned> mMouse;
    std::map<std::uint32_t, std::set<std::string, std::less<>>> mSkip;
};
#endif
