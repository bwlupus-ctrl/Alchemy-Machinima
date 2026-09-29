/**
 * @file alavatarlightresponse.h
 * @brief [AvatarLightResponse] Per-avatar, light-independent response trim
 *        (Diffuse / Tame reflections / Brightness / Glow): the target store.
 *
 * Holds the explicit per-target entries (resident, self, animesh, Ghost Studio
 * clone), the per-frame draw-time lookup table, the counters behind the
 * UPLOADED / VISIBLE instrument, persistence (per account) and Director scene
 * import / export.  See doc/AVATAR_LIGHT_RESPONSE_DESIGN.md (sections 3, 4, 8, 10).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_AVATAR_LIGHT_RESPONSE_H
#define AL_AVATAR_LIGHT_RESPONSE_H

#include "stdtypes.h"

#include "alavatarlightresponsemodel.h"
#include "lluuid.h"
#include "llsd.h"
#include "v4math.h"

#include <boost/signals2.hpp>

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class LLVOAvatar;

class ALAvatarLightResponse
{
public:
    enum EKind
    {
        KIND_RESIDENT = 0,
        KIND_SELF,
        KIND_ANIMESH,
        KIND_CLONE
    };

    // Presence is meaningful: a present entry (including explicit identity and
    // bypassed entries) terminates the fallback chain; absent = inherit.
    struct Entry
    {
        EKind                   mKind = KIND_RESIDENT;
        std::string             mLabel;
        ALLightResponse::Params mParams;
    };

    struct Slot
    {
        bool      mIdentity = true;
        LLVector4 mPacked = LLVector4(0.f, 1.f, 1.f, 1.f);
        LLUUID    mStoreKey;       // entry this slot originates from (instrument attribution)
    };

    // Result of the on-demand instrument query for one stored target.
    struct TargetStatus
    {
        S32  mUploads = 0;         // non-identity uploads last frame (consumer program bound)
        S32  mVisible = -1;        // 1 visible, 0 not visible, -1 cannot be determined
        bool mMuted = false;       // visually muted / jellydolled (expected no-trim)
        bool mResolved = false;    // key resolved to a live avatar
    };

    // Set the first time any non-identity value is set or loaded; sticky for
    // the session.  Checked before instance() so a fresh session adds zero GL calls.
    static bool sEverActive;

    static ALAvatarLightResponse& instance();

    // ---- target identity (design section 3) ----
    // self -> gAgentID; ghost -> Studio instance id; control avatar -> actor owner id; else getID().
    static LLUUID keyForAvatar(const LLVOAvatar* av, EKind* out = nullptr);
    // True when the effective slot for this avatar is present and non-identity.
    // Used by LLVOAvatar::computeUpdatePeriod (design section 2.3).
    static bool keepsLive(const LLVOAvatar* av);
    // Per-frame hook (llappviewer idle, after the object list update): rebuilds
    // the table, rolls the counters and emits the 1 Hz debug log.
    static void tick();

    // ---- entries ----
    const Entry* findEntry(const LLUUID& key) const;
    const std::map<LLUUID, Entry>& entries() const { return mEntries; }
    // Sanitizes; keeps identity.  save_now = false for continuous slider drags.
    // Returns false (and changes nothing) when a NEW stored entry would exceed the capacity
    // (maxStoredEntries(), clones excluded: they are session-only; your own entry is exempt).
    bool set(const LLUUID& key, EKind kind, const std::string& label,
             const ALLightResponse::Params& params, bool save_now = true);
    static size_t maxStoredEntries();
    size_t storedEntryCount() const;    // non-clone entries (the ones that are saved)
    void inherit(const LLUUID& key, bool save_now = true);
    void clearAll(bool save_now = true);

    // ---- draw-time lookup (first non-null key with a slot terminates) ----
    // lookup: nullptr for absent OR identity.  peek: identical, kept as a
    // separate name for the non-counting call sites.
    const LLVector4* lookup(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3, LLUUID* hit_key);
    const LLVector4* peek(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3);
    void noteUpload(const LLUUID& hit_key);

    // ---- instrument / UI support ----
    TargetStatus queryStatus(const LLUUID& key) const;
    S32  getUploadsLastFrame(const LLUUID& key) const;
    S32  countLiveOverBudget() const;   // adjusted, visible, non-muted avatars beyond the impostor limit
    S32  countAdjustedVisible() const;

    // ---- persistence ----
    // Always resets the account-owned store first (account switch / failed login / missing file),
    // then merges the account's file when persistence is on.  Call once gAgentID is known.
    void loadFromFile();
    void saveToFile() const;      // refuses to write while gAgentID is null (self key would be lost)
    void saveIfPersist() const;

    // ---- Director scene I/O ----
    // ids: cast member ids; a null id means "You" and is written as "self".
    LLSD exportFor(const std::vector<LLUUID>& ids) const;
    // Returns true when at least one entry was merged. Entry count and label length are capped.
    bool importMerge(const LLSD& data);

    // ---- change notification ----
    typedef boost::signals2::signal<void()> change_signal_t;
    boost::signals2::connection setChangeCallback(const change_signal_t::slot_type& cb)
    {
        return mChangeSignal.connect(cb);
    }

    static std::string keyToString(const LLUUID& key);
    static LLUUID      keyFromString(const std::string& str);

private:
    ALAvatarLightResponse();
    ALAvatarLightResponse(const ALAvatarLightResponse&) = delete;
    ALAvatarLightResponse& operator=(const ALAvatarLightResponse&) = delete;

    void ensureTable();
    const Slot* findSlot(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3, LLUUID* hit_key);
    void markChanged(const LLUUID& key);
    void dirtyImpostors(const LLUUID& key) const;
    void debugLogTick();

    std::map<LLUUID, Entry>            mEntries;
    std::unordered_map<LLUUID, Slot>   mTable;
    std::unordered_map<LLUUID, S32>    mUploadsNow;
    std::unordered_map<LLUUID, S32>    mUploadsLast;
    std::unordered_set<LLUUID>         mLiveWearers;   // wearers kept live because a worn animesh is adjusted
    bool                               mDirty = true;
    U32                                mTableFrame = 0xFFFFFFFFu;
    F64                                mLastLogTime = 0.0;
    change_signal_t                    mChangeSignal;
};

#endif // AL_AVATAR_LIGHT_RESPONSE_H
