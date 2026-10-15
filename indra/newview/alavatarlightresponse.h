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
#include <set>
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
        bool                       mIdentity = true;
        ALLightResponse::Packed    mPacked;
        LLUUID                     mStoreKey;       // entry this slot originates from (instrument attribution)
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
    // Frame-local silhouette summary. Bits are documented in
    // AVATAR_SILHOUETTE_DESIGN.md section 5.2.
    static U32 sSilFrameMask;

    static ALAvatarLightResponse& instance();
    static bool instanceExists();

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
    // Sanitizes; keeps identity.  save_now = false for continuous slider drags (the caller flushes later).
    // Returns false (and changes nothing) when a NEW stored entry would exceed the capacity
    // (maxStoredEntries(), clones excluded: they are session-only; your own entry is exempt).
    bool set(const LLUUID& key, EKind kind, const std::string& label,
             const ALLightResponse::Params& params, bool save_now = true);
    static size_t maxStoredEntries();
    size_t storedEntryCount() const;    // non-clone live entries plus file-only extras (the ones that are saved)
    void inherit(const LLUUID& key, bool save_now = true);
    // user_action = true only for the explicit "Reset all" confirmation: it counts as an edit even on an empty store.
    void clearAll(bool save_now = true, bool user_action = false);

    // ---- draw-time lookup (first non-null key with a slot terminates) ----
    // lookup: nullptr for absent OR identity.  peek: identical, kept as a
    // separate name for the non-counting call sites.
    const ALLightResponse::Packed* lookup(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3, LLUUID* hit_key);
    const ALLightResponse::Packed* peek(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3);
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
    // THE one write path (quit, close, hide, debounce, every edit). Does nothing unless Persist is on, the
    // store holds an unsaved user edit, and gAgentID is the account loadFromFile ran for. A file that could
    // not be read at load is only rewritten after an explicit edit, and is copied to a timestamped .bak- name first.
    void flushIfDirty();
    bool hasUnsavedChanges() const { return mSaveDirty; }
    // Fired after each successful write, so UI debounce flags can be cleared (one edit = one write).
    typedef boost::signals2::signal<void()> saved_signal_t;
    boost::signals2::connection setSavedCallback(const saved_signal_t::slot_type& cb)
    {
        return mSavedSignal.connect(cb);
    }

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
    U64 revision() const { return mRevision; }

    static std::string keyToString(const LLUUID& key);
    static LLUUID      keyFromString(const std::string& str);

private:
    ALAvatarLightResponse();
    ~ALAvatarLightResponse();
    ALAvatarLightResponse(const ALAvatarLightResponse&) = delete;
    ALAvatarLightResponse& operator=(const ALAvatarLightResponse&) = delete;

    void ensureTable();
    const Slot* findSlot(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3, LLUUID* hit_key);
    void markChanged(const LLUUID& key);
    void dirtyImpostors(const LLUUID& key) const;
    void debugLogTick();
    void captureFileExtras();   // ON transition with persistence OFF at login: file-only entries, kept out of mEntries
    // Admission shared by the account file load and Director scene import. Entries over the cap are handed
    // to `overflow` (when given) instead of being dropped.
    bool mergeEntries(const LLSD& data, std::map<LLUUID, LLSD>* overflow);
    void markUserEdit();        // an edit that changes what the file should hold
    bool saveToFile();          // gated; see flushIfDirty()

    std::map<LLUUID, Entry>            mEntries;
    // File-only entries (raw saved LLSD) captured when persistence is switched ON mid-session. They are
    // written back on every save but never enter mEntries, so the live view is unchanged.
    std::map<LLUUID, LLSD>             mPersistExtras;
    std::set<LLUUID>                   mPersistSuppressed;   // explicitly cleared this session: never resurrected from the file
    bool                               mPersistSuppressAll = false;   // "Reset all" this session: no file-only entries

    // Save gating (see flushIfDirty): never write an account file the session did not load or cannot read.
    bool                               mSaveDirty = false;         // user edit not yet on disk
    LLUUID                             mLoadedForAgent;            // account loadFromFile ran for (null = none yet)
    bool                               mLoadFailed = false;        // an existing file could not be read
    bool                               mEditedSinceLoadFail = false;
    bool                               mBackupWarned = false;      // the "backup failed" warning was logged (reset on success)
    bool                               mSaveFailWarned = false;    // the "save failed" warning was logged once (reset on success)
    saved_signal_t                     mSavedSignal;

    std::unordered_map<LLUUID, Slot>   mTable;
    std::unordered_map<LLUUID, S32>    mUploadsNow;
    std::unordered_map<LLUUID, S32>    mUploadsLast;
    std::unordered_set<LLUUID>         mLiveWearers;   // wearers kept live because a worn animesh is adjusted
    bool                               mDirty = true;
    U32                                mTableFrame = 0xFFFFFFFFu;
    F64                                mLastLogTime = 0.0;
    U64                                mRevision = 1;
    change_signal_t                    mChangeSignal;
};

#endif // AL_AVATAR_LIGHT_RESPONSE_H
