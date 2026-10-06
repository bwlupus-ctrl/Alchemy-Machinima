/**
 * @file alclientheight.h
 * @brief [ClientHeight] Client-side avatar height: the session-only store of
 *        local, render-only vertical offsets, one per avatar key.
 *
 * Never saved, never sent to the simulator. The key convention is the Light
 * Response one (alavatarkey.h). See doc/CLIENT_AVATAR_HEIGHT_DESIGN.md.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_CLIENT_HEIGHT_H
#define AL_CLIENT_HEIGHT_H

#include "alavatarkey.h"
#include "alclientheightmodel.h"
#include "llsingleton.h"
#include "lluuid.h"

#include <boost/signals2.hpp>

class LLVOAvatar;

class ALClientHeight final : public LLSingleton<ALClientHeight>
{
    LLSINGLETON(ALClientHeight);

public:
    typedef boost::signals2::signal<void()> change_signal_t;

    // The store key for an avatar. Prefers the stable id Ghost Studio stamped on
    // a clone (so the first foot lock after a respawn already resolves).
    static LLUUID keyForAvatar(const LLVOAvatar* av, ALAvatarKey::EKind* out_kind = nullptr);

    F32  get(const LLUUID& key) const { return mStore.get(key); }
    // Return true on a real change; the change signal fires only then.
    bool set(const LLUUID& key, F32 z);
    bool reset(const LLUUID& key);
    bool resetAll();
    // The LLAppViewer::disconnectViewer path: offsets must not leak across accounts.
    void clearSession() { resetAll(); }

    bool   empty() const { return mStore.empty(); }
    size_t size() const { return mStore.size(); }
    U64    revision() const { return mStore.revision(); }
    const ALClientHeightModel::Store& store() const { return mStore; }

    boost::signals2::connection setChangeCallback(const change_signal_t::slot_type& cb)
    {
        return mChangeSignal.connect(cb);
    }

private:
    ALClientHeightModel::Store mStore;
    change_signal_t            mChangeSignal;
};

#endif // AL_CLIENT_HEIGHT_H
