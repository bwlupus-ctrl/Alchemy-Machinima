/**
 * @file alavatarlightresponse.cpp
 * @brief [AvatarLightResponse] Per-avatar, light-independent response trim:
 *        the target store, draw-time lookup table, persistence and instrument.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alavatarlightresponse.h"

#include "alavatarkey.h"   // [ClientHeight] shared key helper
#include "alghoststudio.h"
#include "llagentdata.h"
#include "llapp.h"
#include "llappviewer.h"
#include "llcontrolavatar.h"
#include "llcharacter.h"
#include "lldir.h"
#include "llfile.h"
#include "llghostavatar.h"
#include "llgl.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "lltimer.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <system_error>

namespace
{
const char* const ALR_FILE_NAME = "avatar_light_response.xml";
const char* const ALR_SELF_KEY  = "self";
const size_t ALR_MAX_ENTRIES = 512;   // file / scene import cap (untrusted input)
const S32    ALR_MAX_LABEL   = 64;    // bytes

std::string kindToString(ALAvatarLightResponse::EKind kind)
{
    switch (kind)
    {
    case ALAvatarLightResponse::KIND_SELF:    return "self";
    case ALAvatarLightResponse::KIND_ANIMESH: return "animesh";
    case ALAvatarLightResponse::KIND_CLONE:   return "clone";
    default:                                  return "resident";
    }
}

ALAvatarLightResponse::EKind kindFromString(const std::string& str)
{
    if (str == "self")    return ALAvatarLightResponse::KIND_SELF;
    if (str == "animesh") return ALAvatarLightResponse::KIND_ANIMESH;
    if (str == "clone")   return ALAvatarLightResponse::KIND_CLONE;
    return ALAvatarLightResponse::KIND_RESIDENT;
}

// ONE priority order for admission (file load, scene import) when the cap bites:
// 0 = your own entry, 1 = explicit identity / bypass (terminates fallback chains), 2 = everything else.
int persistPriority(const LLUUID& key, const ALLightResponse::Params& params)
{
    if (key == gAgentID)
    {
        return 0;
    }
    return ALLightResponse::isIdentity(params) ? 1 : 2;
}

// Copies an unreadable account file to a fresh name: base-YYYYMMDD-HHMMSS, then base-YYYYMMDD-HHMMSS-2, -3, ...
// Each copy refuses to overwrite (copy_options::none), so a name taken in the meantime is never clobbered.
// Returns false, with nothing written, when no backup can be made.
bool backupUnreadableFile(const std::string& path, std::string& backup_out)
{
    try
    {
        const std::time_t now = std::time(nullptr);
        char stamp[32] = "";
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
        const std::string base = path + ".bak-" + stamp;
        const S64 source_size = LLFile::size(path);
        for (int n = 1; n <= 99; ++n)
        {
            const std::string candidate = n == 1 ? base : base + "-" + std::to_string(n);
            if (LLFile::exists(candidate))
            {
                continue;
            }
            std::error_code ec;
            if (LLFile::copy(path, candidate, std::filesystem::copy_options::none, ec))
            {
                if (LLFile::size(candidate) != source_size)
                {
                    return false;   // the copy must match what it backs up
                }
                backup_out = candidate;
                return true;
            }
            if (ec != std::errc::file_exists)
            {
                return false;   // a real failure, not a name collision: retrying other names will not help
            }
        }
        return false;
    }
    catch (...)
    {
        LL_WARNS("AvatarLightResponse") << "Backup of \"" << path << "\" failed with an exception" << LL_ENDL;
        return false;
    }
}

// Best-effort removal of a temp file. Never throws; a failure is logged and otherwise ignored.
void removeQuietly(const std::string& path)
{
    try
    {
        LLFile::remove(path, ENOENT);
    }
    catch (...)
    {
        LL_WARNS("AvatarLightResponse") << "Could not remove the temporary file \"" << path << "\"" << LL_ENDL;
    }
}

// True only for exactly "<target>.tmp-<digits>-<digits>", the name saveToFile() creates (pid, then counter).
bool isSaveTempName(const std::string& name, const std::string& target)
{
    const std::string prefix = target + ".tmp-";
    if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }
    const std::string rest = name.substr(prefix.size());
    const std::string::size_type dash = rest.find('-');
    if (dash == std::string::npos || dash == 0)
    {
        return false;
    }
    const std::string pid = rest.substr(0, dash);
    const std::string counter = rest.substr(dash + 1);
    const auto all_digits = [](const std::string& s)
    {
        return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
    };
    return all_digits(pid) && all_digits(counter);
}

// Best effort, at load: removes temp files stranded by an interrupted save ("<file>.tmp-<pid>-<n>") that are
// older than 10 minutes, so a live viewer's in-flight temp is never touched. Never throws.
void removeStaleSaveFiles(const std::string& path)
{
    try
    {
        const std::filesystem::path file(path);
        std::error_code ec;
        const std::filesystem::directory_iterator end;
        for (std::filesystem::directory_iterator it(file.parent_path(), ec); !ec && it != end; it.increment(ec))
        {
            const std::filesystem::path entry = it->path();
            const std::string name = entry.filename().string();
            if (!isSaveTempName(name, ALR_FILE_NAME))
            {
                continue;
            }
            std::error_code mtime_ec;
            const std::filesystem::file_time_type mtime = std::filesystem::last_write_time(entry, mtime_ec);
            if (mtime_ec)
            {
                continue;   // unknown age: leave it alone
            }
            if (std::filesystem::file_time_type::clock::now() - mtime < std::chrono::minutes(10))
            {
                continue;   // possibly another viewer's save in progress
            }
            LLFile::remove(entry, ENOENT);
        }
    }
    catch (...)
    {
        LL_WARNS("AvatarLightResponse") << "Could not clean up stale save files next to \"" << path << "\"" << LL_ENDL;
    }
}
}

bool ALAvatarLightResponse::sEverActive = false;
U32 ALAvatarLightResponse::sSilFrameMask = 0;
static ALAvatarLightResponse* sAvatarLightResponseInstance = nullptr;

// static
ALAvatarLightResponse& ALAvatarLightResponse::instance()
{
    static ALAvatarLightResponse sInstance;
    sAvatarLightResponseInstance = &sInstance;
    return sInstance;
}

bool ALAvatarLightResponse::instanceExists()
{
    return sAvatarLightResponseInstance != nullptr;
}

ALAvatarLightResponse::ALAvatarLightResponse()
{
    // Master toggle: the table empties/refills on the next frame and every
    // avatar rebakes its impostor so nothing keeps a stale (un)trimmed bake.
    LLControlVariable* master = gSavedSettings.getControl("AvatarLightResponseEnabled");
    if (master)
    {
        master->getCommitSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&)
            {
                mDirty = true;
                LLVOAvatar::resetImpostors();
                mChangeSignal();
            });
    }

    // Persistence toggle. ON: file-only entries are kept as extras (written, never shown) and the
    // table is saved now. OFF: no further saves; the file is left as is.
    LLControlVariable* persist = gSavedSettings.getControl("AvatarLightResponsePersist");
    if (persist)
    {
        persist->getCommitSignal()->connect(
            [this](LLControlVariable*, const LLSD& newval, const LLSD&)
            {
                if (!newval.asBoolean())
                {
                    // OFF: file-only extras stop counting toward the cap. The suppression sets stay: they
                    // record targets cleared this session that the (unrewritten) file still holds.
                    mPersistExtras.clear();
                    return;
                }
                captureFileExtras();
                mSaveDirty = true;   // the switch itself publishes the live table (not an edit of a file we could not read)
                flushIfDirty();
            });
    }
}

ALAvatarLightResponse::~ALAvatarLightResponse()
{
    sAvatarLightResponseInstance = nullptr;
}

// static
std::string ALAvatarLightResponse::keyToString(const LLUUID& key)
{
    if (key == gAgentID)
    {
        return ALR_SELF_KEY;
    }
    return key.asString();
}

// static
LLUUID ALAvatarLightResponse::keyFromString(const std::string& str)
{
    if (str == ALR_SELF_KEY)
    {
        return gAgentID;
    }
    LLUUID id;
    if (!id.set(str, false))
    {
        return LLUUID::null;
    }
    return id;
}

// static
LLUUID ALAvatarLightResponse::keyForAvatar(const LLVOAvatar* av, EKind* out)
{
    // [ClientHeight] The convention now lives in the shared helper (alavatarkey.h);
    // use_clone_stamp = false keeps the historical behaviour exactly.
    ALAvatarKey::EKind kind = ALAvatarKey::KIND_RESIDENT;
    const LLUUID key = ALAvatarKey::forAvatar(av, &kind, false);
    if (out)
    {
        switch (kind)
        {
        case ALAvatarKey::KIND_SELF:    *out = KIND_SELF;    break;
        case ALAvatarKey::KIND_ANIMESH: *out = KIND_ANIMESH; break;
        case ALAvatarKey::KIND_CLONE:   *out = KIND_CLONE;   break;
        default:                        *out = KIND_RESIDENT; break;
        }
    }
    return key;
}

// static
bool ALAvatarLightResponse::keepsLive(const LLVOAvatar* av)
{
    if (!sEverActive || !av)
    {
        return false;
    }
    const LLUUID k1 = keyForAvatar(av, nullptr);
    LLUUID k2;
    if (av->isControlAvatar())
    {
        if (const LLVOAvatar* wearer = av->getAttachedAvatar())
        {
            k2 = wearer->getID();
        }
    }
    ALAvatarLightResponse& store = instance();
    store.ensureTable();
    const Slot* slot = store.findSlot(k1, k2, LLUUID::null, nullptr);
    if (slot && !slot->mIdentity)
    {
        return true;
    }
    // A wearer with an adjusted worn animesh stays live: LLControlAvatar::isImpostor() follows the
    // wearer's impostor state, so an impostored wearer would bake the pet untrimmed.
    return !store.mLiveWearers.empty() && store.mLiveWearers.find(k1) != store.mLiveWearers.end();
}

// static
void ALAvatarLightResponse::tick()
{
    if (!sEverActive)
    {
        return;
    }
    ALAvatarLightResponse& store = instance();
    store.ensureTable();
    if ((sSilFrameMask & 1u) &&
        !gSavedSettings.getBOOL("RenderEnableEmissiveBuffer") &&
        !gSavedSettings.getBOOL("AvatarSilhouetteForceEmissive"))
    {
        // Session-sticky: the non-persistent setting is cleared on login and
        // forcing it once avoids rebuilding targets and shaders on every edit.
        gSavedSettings.setBOOL("AvatarSilhouetteForceEmissive", true);
    }
    store.debugLogTick();
}

const ALAvatarLightResponse::Entry* ALAvatarLightResponse::findEntry(const LLUUID& key) const
{
    const auto it = mEntries.find(key);
    return it != mEntries.end() ? &it->second : nullptr;
}

// static
size_t ALAvatarLightResponse::maxStoredEntries()
{
    return ALR_MAX_ENTRIES;
}

// Live entries plus file-only extras: the one count every capacity check uses.
size_t ALAvatarLightResponse::storedEntryCount() const
{
    size_t count = 0;
    for (const auto& kv : mEntries)
    {
        if (kv.second.mKind != KIND_CLONE)
        {
            ++count;
        }
    }
    for (const auto& kv : mPersistExtras)
    {
        if (mEntries.find(kv.first) == mEntries.end())
        {
            ++count;
        }
    }
    return count;
}

bool ALAvatarLightResponse::set(const LLUUID& key, EKind kind, const std::string& label,
                                const ALLightResponse::Params& params, bool save_now)
{
    if (key.isNull())
    {
        return false;
    }
    // ONE capacity policy for set / load / scene import / save: a new stored entry is refused past the cap
    // (own entry exempt so "You" can never be the one that is dropped).
    const bool known = mEntries.find(key) != mEntries.end() || mPersistExtras.find(key) != mPersistExtras.end();
    if (kind != KIND_CLONE && key != gAgentID && !known && storedEntryCount() >= ALR_MAX_ENTRIES)
    {
        return false;
    }
    Entry entry;
    entry.mKind   = kind;
    entry.mLabel  = utf8str_truncate(label, ALR_MAX_LABEL);
    entry.mParams = ALLightResponse::sanitize(params);
    mEntries[key] = entry;
    mPersistExtras.erase(key);   // the live entry now owns this target
    if (!ALLightResponse::isIdentity(entry.mParams))
    {
        sEverActive = true;
    }
    markChanged(key);
    if (kind != KIND_CLONE)
    {
        markUserEdit();
        if (save_now)
        {
            flushIfDirty();
        }
    }
    return true;
}

void ALAvatarLightResponse::inherit(const LLUUID& key, bool save_now)
{
    // An explicit clear removes the file-only copy too, so no later save can bring it back.
    mPersistSuppressed.insert(key);
    const bool wasExtra = mPersistExtras.erase(key) > 0;
    if (wasExtra)
    {
        markUserEdit();
    }
    const auto it = mEntries.find(key);
    if (it == mEntries.end())
    {
        if (wasExtra && save_now)
        {
            flushIfDirty();
        }
        return;
    }
    const EKind kind = it->second.mKind;
    mEntries.erase(it);
    markChanged(key);
    if (kind != KIND_CLONE)
    {
        markUserEdit();
        if (save_now)
        {
            flushIfDirty();
        }
    }
}

void ALAvatarLightResponse::clearAll(bool save_now, bool user_action)
{
    // "Reset all" removes file-only entries as well, for the rest of this session.
    mPersistSuppressAll = true;
    bool hadStored = !mPersistExtras.empty();   // anything the file holds that this call removes
    const bool hadExtras = hadStored;
    mPersistExtras.clear();
    for (const auto& kv : mEntries)
    {
        if (kv.second.mKind != KIND_CLONE)
        {
            hadStored = true;
            break;
        }
    }
    // A confirmed reset by the user is an edit even when the table is already empty (e.g. after an unreadable
    // file): it is what lets the empty store be saved. Internal calls do not count as an edit.
    if (hadStored || user_action)
    {
        markUserEdit();
    }
    if (mEntries.empty())
    {
        if ((hadExtras || user_action) && save_now)
        {
            flushIfDirty();
        }
        return;
    }
    mEntries.clear();
    mDirty = true;
    ++mRevision;
    LLVOAvatar::resetImpostors();
    mChangeSignal();
    if (save_now)
    {
        flushIfDirty();
    }
}

void ALAvatarLightResponse::markUserEdit()
{
    mSaveDirty = true;
    mEditedSinceLoadFail = true;
}

void ALAvatarLightResponse::markChanged(const LLUUID& key)
{
    mDirty = true;
    ++mRevision;
    dirtyImpostors(key);
    mChangeSignal();
}

// Any avatar whose effective response could have changed rebakes its impostor
// (design section 4.1): the target itself, an animesh linkset it owns, and
// control avatars worn by the target (they inherit the wearer's entry).
void ALAvatarLightResponse::dirtyImpostors(const LLUUID& key) const
{
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* av = static_cast<LLVOAvatar*>(character);
        if (!av || av->isDead())
        {
            continue;
        }
        bool hit = (av->getID() == key);
        if (!hit && av->isGhostAvatar())
        {
            const ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().findInstanceByRuntime(av->getID());
            hit = inst && (inst->mId == key || inst->mSource == key || (inst->mSource.isNull() && key == gAgentID));
        }
        if (!hit && av->isControlAvatar())
        {
            hit = (av->getActorFxOwnerId() == key);
            if (!hit)
            {
                if (const LLVOAvatar* wearer = av->getAttachedAvatar())
                {
                    hit = (wearer->getID() == key);
                }
            }
        }
        if (hit)
        {
            av->mNeedsImpostorUpdate = true;
        }
    }
}

void ALAvatarLightResponse::ensureTable()
{
    if (!mDirty && mTableFrame == gFrameCount)
    {
        return;
    }
    if (mTableFrame != gFrameCount)
    {
        mUploadsLast.swap(mUploadsNow);
        mUploadsNow.clear();
    }
    mTableFrame = gFrameCount;
    mDirty = false;
    mTable.clear();
    mLiveWearers.clear();
    sSilFrameMask = 0;

    static LLCachedControl<bool> master(gSavedSettings, "AvatarLightResponseEnabled", true);
    if (!master() || mEntries.empty())
    {
        return;
    }

    static LLCachedControl<bool> has_hdr(gSavedSettings, "RenderHDREnabled", true);
    const bool hdr = has_hdr() && gGLManager.mGLVersion > 4.05f;

    for (const auto& kv : mEntries)
    {
        Slot slot;
        slot.mIdentity = ALLightResponse::isIdentity(kv.second.mParams);
        slot.mPacked   = ALLightResponse::packAll(kv.second.mParams, hdr);
        slot.mStoreKey = kv.first;
        mTable[kv.first] = slot;

        if (slot.mPacked.mSil.mV[VW] > 0.f)
        {
            sSilFrameMask |= 1u;
            if (slot.mPacked.mSilAux.mV[VY] < 0.5f) sSilFrameMask |= 2u;
            if (slot.mPacked.mSilAux.mV[VZ] < 0.5f) sSilFrameMask |= 4u;
        }
    }

    if (sSilFrameMask & 1u)
    {
        static LLCachedControl<bool> debug_carrier(gSavedSettings, "AvatarSilhouetteDebugCarrier", false);
        if (debug_carrier()) sSilFrameMask |= 8u;
        if (hdr)             sSilFrameMask |= 16u;
    }

    // Entity clones are drawn as avatars keyed by their runtime id: map that id
    // to the clone's own slot, else to its source's slot (null source = self).
    const std::vector<ALGhostStudio::Instance>& instances = ALGhostStudio::instance().getInstances();
    for (const ALGhostStudio::Instance& inst : instances)
    {
        if (inst.mKind != ALGhostStudio::BACKING_ENTITY_CLONE || inst.mEntityId.isNull())
        {
            continue;
        }
        if (mTable.find(inst.mEntityId) != mTable.end())
        {
            continue;
        }
        auto it = mTable.find(inst.mId);
        if (it == mTable.end())
        {
            it = mTable.find(inst.mSource.isNull() ? gAgentID : inst.mSource);
        }
        if (it != mTable.end())
        {
            const Slot copy = it->second;
            mTable[inst.mEntityId] = copy;
        }
    }

    // Wearers of adjusted worn animesh (see keepsLive).
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* av = static_cast<LLVOAvatar*>(character);
        if (!av || av->isDead() || !av->isControlAvatar())
        {
            continue;
        }
        const LLVOAvatar* wearer = av->getAttachedAvatar();
        if (!wearer)
        {
            continue;
        }
        const Slot* slot = nullptr;
        const LLUUID own = av->getActorFxOwnerId();
        const auto own_it = mTable.find(own);
        if (own_it != mTable.end())
        {
            slot = &own_it->second;
        }
        else
        {
            const auto wearer_it = mTable.find(wearer->getID());
            if (wearer_it != mTable.end())
            {
                slot = &wearer_it->second;
            }
        }
        if (slot && !slot->mIdentity)
        {
            mLiveWearers.insert(wearer->getID());
        }
    }
}

const ALAvatarLightResponse::Slot* ALAvatarLightResponse::findSlot(const LLUUID& k1, const LLUUID& k2,
                                                                    const LLUUID& k3, LLUUID* hit_key)
{
    ensureTable();
    if (mTable.empty())
    {
        return nullptr;
    }
    const LLUUID* keys[3] = { &k1, &k2, &k3 };
    for (const LLUUID* key : keys)
    {
        if (key->isNull())
        {
            continue;
        }
        const auto it = mTable.find(*key);
        if (it != mTable.end())
        {
            if (hit_key)
            {
                *hit_key = it->second.mStoreKey;
            }
            return &it->second;   // explicit entry (identity included) terminates the chain
        }
    }
    return nullptr;
}

const ALLightResponse::Packed* ALAvatarLightResponse::lookup(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3,
                                                             LLUUID* hit_key)
{
    const Slot* slot = findSlot(k1, k2, k3, hit_key);
    return (slot && !slot->mIdentity) ? &slot->mPacked : nullptr;
}

const ALLightResponse::Packed* ALAvatarLightResponse::peek(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3)
{
    const Slot* slot = findSlot(k1, k2, k3, nullptr);
    return (slot && !slot->mIdentity) ? &slot->mPacked : nullptr;
}

void ALAvatarLightResponse::noteUpload(const LLUUID& hit_key)
{
    if (hit_key.notNull())
    {
        ++mUploadsNow[hit_key];
    }
}

S32 ALAvatarLightResponse::getUploadsLastFrame(const LLUUID& key) const
{
    const auto it = mUploadsLast.find(key);
    return it != mUploadsLast.end() ? it->second : 0;
}

ALAvatarLightResponse::TargetStatus ALAvatarLightResponse::queryStatus(const LLUUID& key) const
{
    TargetStatus st;
    st.mUploads = getUploadsLastFrame(key);

    const Entry* entry = findEntry(key);
    const EKind kind = entry ? entry->mKind : KIND_RESIDENT;

    LLVOAvatar* av = nullptr;
    if (kind == KIND_CLONE)
    {
        ALGhostStudio& studio = ALGhostStudio::instance();
        const ALGhostStudio::Instance* inst = studio.getInstance(key);
        if (inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            av = studio.resolveEntityClone(key);
        }
        // Overlay clones have no avatar to interrogate: visibility stays unknown.
    }
    else
    {
        LLViewerObject* obj = gObjectList.findObject(key);
        if (obj)
        {
            av = obj->asAvatar();
            if (!av)
            {
                av = obj->getControlAvatar();
            }
        }
    }
    if (av && !av->isDead())
    {
        st.mResolved = true;
        st.mVisible  = av->isVisible() ? 1 : 0;
        st.mMuted    = av->isVisuallyMuted();
    }
    return st;
}

// Note: adjusted animesh (control avatars) are counted here too. That is intentional -- they also
// bypass the impostor limit through keepsLive() -- so the floater's budget warning includes them.
S32 ALAvatarLightResponse::countAdjustedVisible() const
{
    if (!sEverActive)
    {
        return 0;
    }
    S32 count = 0;
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* av = static_cast<LLVOAvatar*>(character);
        if (!av || av->isDead() || av->isGhostAvatar() || av->isSelf())
        {
            continue;
        }
        if (keepsLive(av) && av->isVisible() && !av->isVisuallyMuted())
        {
            ++count;
        }
    }
    return count;
}

S32 ALAvatarLightResponse::countLiveOverBudget() const
{
    if (!LLVOAvatar::sLimitNonImpostors)
    {
        return 0;
    }
    const S32 count = countAdjustedVisible();
    const S32 limit = static_cast<S32>(LLVOAvatar::sMaxNonImpostors);
    return count > limit ? count - limit : 0;
}

void ALAvatarLightResponse::debugLogTick()
{
    static LLCachedControl<bool> debug_log(gSavedSettings, "AvatarLightResponseDebugLog", false);
    if (!debug_log())
    {
        return;
    }
    const F64 now = LLTimer::getTotalSeconds();
    if (now - mLastLogTime < 1.0)
    {
        return;
    }
    mLastLogTime = now;

    S32 active = 0;
    for (const auto& kv : mEntries)
    {
        if (!ALLightResponse::isIdentity(kv.second.mParams))
        {
            ++active;
        }
    }
    LL_INFOS("AvatarLightResponse") << "[ALR] targets=" << mTable.size()
        << " explicit=" << mEntries.size()
        << " active=" << active
        << " live_over_budget=" << countLiveOverBudget()
        << " sil_mask=" << sSilFrameMask << LL_ENDL;

    for (const auto& kv : mEntries)
    {
        if (ALLightResponse::isIdentity(kv.second.mParams))
        {
            continue;
        }
        const TargetStatus st = queryStatus(kv.first);
        const char* verdict = "INCONCLUSIVE-NOT-VISIBLE";
        if (st.mMuted)
        {
            verdict = "EXPECTED-MUTED";
        }
        else if (st.mVisible == 1 && st.mUploads == 0)
        {
            verdict = "FAIL-NOT-UPLOADED";
        }
        else if (st.mVisible == 1 && st.mUploads > 0)
        {
            verdict = "UPLOADED";
        }
        else if (st.mVisible < 0)
        {
            verdict = "INCONCLUSIVE-VISIBILITY-UNKNOWN";   // overlay clones: no avatar to interrogate
        }
        LL_INFOS("AvatarLightResponse") << "[ALR] " << kv.second.mLabel
            << " key=" << kv.first
            << " uploads=" << st.mUploads
            << " visible=" << st.mVisible
            << " muted=" << (st.mMuted ? 1 : 0)
            << " sil=" << ALLightResponse::packAll(kv.second.mParams,
                    (sSilFrameMask & 16u) != 0).mSil.mV[VW]
            << " fill=" << llformat("#%02X%02X%02X",
                    static_cast<U32>(ll_round(llclamp(kv.second.mParams.mSilFillSrgb.mV[VX], 0.f, 1.f) * 255.f)),
                    static_cast<U32>(ll_round(llclamp(kv.second.mParams.mSilFillSrgb.mV[VY], 0.f, 1.f) * 255.f)),
                    static_cast<U32>(ll_round(llclamp(kv.second.mParams.mSilFillSrgb.mV[VZ], 0.f, 1.f) * 255.f)))
            << " haze=" << (kv.second.mParams.mSilHaze ? 1 : 0)
            << " rim=" << (kv.second.mParams.mSilKeepRim ? 1 : 0)
            << " VERDICT=" << verdict << LL_ENDL;
    }
}

// Called on the OFF -> ON transition. Reads the account file and keeps the entries the session does not
// hold (not cleared or reset this session) as file-only extras. ALL of them are kept, past the cap too: the
// cap limits what the session may ADD, never what a save drops. Live entries are never touched, so the user
// sees no change at the switch itself.
void ALAvatarLightResponse::captureFileExtras()
{
    mPersistExtras.clear();
    if (mPersistSuppressAll || gAgentID.isNull())
    {
        // Reset all this session: nothing from the file. No account yet: loadFromFile will load it.
        return;
    }
    const std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, ALR_FILE_NAME);
    if (!LLFile::exists(path))
    {
        return;
    }
    llifstream in(path.c_str());
    LLSD root;
    S32 ret = LLSDParser::PARSE_FAILURE;
    if (in.is_open())
    {
        ret = LLSDSerialize::fromXML(root, in);
        in.close();
    }
    if (ret == LLSDParser::PARSE_FAILURE || !root.isMap() || !root.has("entries") || !root["entries"].isMap())
    {
        // An existing file we cannot read: never rewrite it without an explicit edit, and back it up first.
        LL_WARNS("AvatarLightResponse") << "Cannot read \"" << path << "\" to keep its entries" << LL_ENDL;
        mLoadFailed = true;
        mEditedSinceLoadFail = false;   // an earlier edit must not let this switch alone overwrite the unreadable file
        return;
    }
    mLoadFailed = false;   // readable now; its entries are kept as extras below
    for (const auto& kv : llsd::inMap(root["entries"]))
    {
        const LLUUID key = keyFromString(kv.first);
        if (key.isNull() || !kv.second.isMap() || kindFromString(kv.second["kind"].asString()) == KIND_CLONE)
        {
            continue;
        }
        if (mEntries.find(key) != mEntries.end() || mPersistSuppressed.find(key) != mPersistSuppressed.end())
        {
            continue;
        }
        mPersistExtras[key] = kv.second;
    }
}

void ALAvatarLightResponse::flushIfDirty()
{
    if (mSaveDirty)
    {
        saveToFile();
    }
}

bool ALAvatarLightResponse::saveToFile()
{
    ALLightResponse::SaveGateInput gate;
    gate.mPersist             = gSavedSettings.getBOOL("AvatarLightResponsePersist");
    gate.mAgentKnown          = gAgentID.notNull();   // "self" cannot be keyed yet: writing now would erase the saved self entry
    gate.mAgentIsLoadedOwner  = gAgentID == mLoadedForAgent;
    gate.mDirty               = mSaveDirty;
    gate.mLoadFailed          = mLoadFailed;
    gate.mEditedSinceLoadFail = mEditedSinceLoadFail;
    if (!ALLightResponse::mayWriteSaveFile(gate))
    {
        return false;
    }
    // Live entries, then file-only extras for targets the table does not hold. Clones are session-only.
    // Nothing is dropped for the cap: extras beyond it came from the file and stay in it.
    LLSD entries = LLSD::emptyMap();
    for (const auto& kv : mEntries)
    {
        if (kv.second.mKind == KIND_CLONE)
        {
            continue;
        }
        LLSD e = ALLightResponse::toLLSD(kv.second.mParams);
        e["kind"]  = kindToString(kv.second.mKind);
        e["label"] = kv.second.mLabel;
        entries[keyToString(kv.first)] = e;
    }
    for (const auto& kv : mPersistExtras)
    {
        if (mEntries.find(kv.first) != mEntries.end())
        {
            continue;   // the live entry is the one that is saved
        }
        entries[keyToString(kv.first)] = kv.second;
    }
    LLSD root = LLSD::emptyMap();
    root["version"] = 1;
    root["entries"] = entries;

    const std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, ALR_FILE_NAME);
    // Unique per process and per save, so two viewers on one account (or two saves in a row) never share a temp file.
    static U32 sSaveCount = 0;
    const std::string tmp_path = path + ".tmp-" + std::to_string(LLApp::getPid()) + "-" + std::to_string(++sSaveCount);
    try
    {
        if (mLoadFailed && LLFile::exists(path))
        {
            // A file this session could not read: keep a copy under a fresh name before the first overwrite.
            // No backup, no write: the edit stays dirty and the warning is given once until a save or load succeeds.
            std::string backup;
            if (!backupUnreadableFile(path, backup))
            {
                if (!mBackupWarned)
                {
                    LL_WARNS("AvatarLightResponse") << "Backup of the unreadable \"" << path
                                                    << "\" failed; leaving it untouched and keeping the edit unsaved" << LL_ENDL;
                    mBackupWarned = true;
                }
                return false;
            }
            LL_WARNS("AvatarLightResponse") << "Saving over an unreadable \"" << path << "\"; keeping the old file as \""
                                            << backup << "\"" << LL_ENDL;
        }
        // Write the whole temp file first; the target is untouched until it is complete.
        {
            llofstream out(tmp_path.c_str());
            if (!out.is_open())
            {
                LL_WARNS("AvatarLightResponse") << "Can't open \"" << tmp_path << "\" for writing" << LL_ENDL;
                return false;
            }
            LLSDSerialize::toPrettyXML(root, out);
            out.flush();
            out.close();
            // Stream state is checked AFTER close: buffered write errors surface there, not before it.
            llstat st;
            const bool written = !out.fail() && LLFile::stat(tmp_path, &st) == 0 && st.st_size > 0;
            if (!written)
            {
                LL_WARNS("AvatarLightResponse") << "Write to \"" << tmp_path << "\" failed; \"" << path
                                                << "\" is unchanged" << LL_ENDL;
                removeQuietly(tmp_path);
                return false;
            }
        }
        // One rename replaces the target in place (LLFile::rename replaces an existing file on Windows).
        // On failure the temp file is removed, the target is untouched and the store stays dirty for a retry.
        if (LLFile::rename(tmp_path, path) != 0)
        {
            removeQuietly(tmp_path);
            if (!mSaveFailWarned)
            {
                LL_WARNS("AvatarLightResponse") << "Could not replace \"" << path
                                                << "\"; the file is unchanged and the edit stays unsaved (retried on the next save)" << LL_ENDL;
                mSaveFailWarned = true;
            }
            return false;
        }
    }
    catch (...)
    {
        removeQuietly(tmp_path);
        if (!mSaveFailWarned)
        {
            LL_WARNS("AvatarLightResponse") << "Saving \"" << path
                                            << "\" failed with an exception; the file is unchanged and the edit stays unsaved" << LL_ENDL;
            mSaveFailWarned = true;
        }
        return false;
    }
    mSaveDirty = false;
    mLoadFailed = false;   // the old file is backed up and replaced; later saves are ordinary
    mBackupWarned = false;
    mSaveFailWarned = false;
    mSavedSignal();
    return true;
}

void ALAvatarLightResponse::loadFromFile()
{
    // Account-owned state never survives an account switch (failed login, relog), whether or not
    // the new account has a file, persistence is on, or the file parses.
    if (!mEntries.empty())
    {
        mEntries.clear();
        LLVOAvatar::resetImpostors();
    }
    mPersistExtras.clear();
    mPersistSuppressed.clear();
    mPersistSuppressAll = false;
    mSaveDirty = false;
    mLoadedForAgent = gAgentID;   // writes are allowed only for the account that was loaded here
    mLoadFailed = false;
    mEditedSinceLoadFail = false;
    mBackupWarned = false;
    mSaveFailWarned = false;
    mUploadsNow.clear();
    mUploadsLast.clear();
    mDirty = true;
    ++mRevision;
    // AvatarSilhouetteForceEmissive is sticky only for one login session.
    gSavedSettings.setBOOL("AvatarSilhouetteForceEmissive", false);
    mChangeSignal();

    if (gAgentID.notNull())
    {
        removeStaleSaveFiles(gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, ALR_FILE_NAME));
    }
    if (!gSavedSettings.getBOOL("AvatarLightResponsePersist"))
    {
        return;
    }
    const std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, ALR_FILE_NAME);
    if (!LLFile::exists(path))
    {
        return;   // first run: nothing saved yet
    }
    llifstream in(path.c_str());
    LLSD root;
    S32 ret = LLSDParser::PARSE_FAILURE;
    if (in.is_open())
    {
        ret = LLSDSerialize::fromXML(root, in);
        in.close();
    }
    if (ret == LLSDParser::PARSE_FAILURE || !root.isMap() || !root.has("entries") || !root["entries"].isMap())
    {
        // An existing file we could not read (parse failure, lock, unreadable): never rewrite it from an
        // empty table. flushIfDirty() waits for an explicit edit and backs the file up first.
        LL_WARNS("AvatarLightResponse") << "Ignoring unreadable \"" << path << "\"" << LL_ENDL;
        mLoadFailed = true;
        return;
    }
    // Entries past the cap stay in the file as file-only extras: a save never truncates it.
    std::map<LLUUID, LLSD> overflow;
    mergeEntries(root, &overflow);
    if (!overflow.empty())
    {
        LL_WARNS("AvatarLightResponse") << "\"" << path << "\" holds " << overflow.size()
                                        << " entries past the " << ALR_MAX_ENTRIES << " cap; keeping them unused" << LL_ENDL;
        for (const auto& kv : overflow)
        {
            mPersistExtras[kv.first] = kv.second;
        }
    }
}

LLSD ALAvatarLightResponse::exportFor(const std::vector<LLUUID>& ids) const
{
    LLSD entries = LLSD::emptyMap();
    for (const LLUUID& id : ids)
    {
        const LLUUID key = id.isNull() ? gAgentID : id;
        const auto it = mEntries.find(key);
        if (it == mEntries.end() || it->second.mKind == KIND_CLONE)
        {
            continue;
        }
        LLSD e = ALLightResponse::toLLSD(it->second.mParams);
        e["kind"]  = kindToString(it->second.mKind);
        e["label"] = it->second.mLabel;
        entries[keyToString(key)] = e;
    }
    LLSD root = LLSD::emptyMap();
    root["version"] = 1;
    root["entries"] = entries;
    return root;
}

bool ALAvatarLightResponse::importMerge(const LLSD& data)
{
    const bool any = mergeEntries(data, nullptr);
    if (any)
    {
        markUserEdit();   // a scene import changes what the account file should hold; the caller flushes
    }
    return any;
}

bool ALAvatarLightResponse::mergeEntries(const LLSD& data, std::map<LLUUID, LLSD>* overflow)
{
    if (!data.isMap() || !data.has("entries") || !data["entries"].isMap())
    {
        return false;
    }
    bool any = false;
    // Three priority passes so that, when the cap bites, the dropped entries are never your own
    // ("self") or explicit identity/bypass entries (which terminate fallback chains):
    // 0 = self, 1 = explicit identity / bypass, 2 = everything else.
    for (int pass = 0; pass < 3; ++pass)
    {
        for (const auto& kv : llsd::inMap(data["entries"]))
        {
            const LLUUID key = keyFromString(kv.first);
            if (key.isNull() || !kv.second.isMap())
            {
                continue;
            }
            EKind kind = kindFromString(kv.second["kind"].asString());
            if (kind == KIND_CLONE)
            {
                continue;
            }
            const ALLightResponse::Params params = ALLightResponse::fromLLSD(kv.second);
            const int priority = persistPriority(key, params);
            if (priority != pass)
            {
                continue;
            }
            const bool known = mEntries.find(key) != mEntries.end() || mPersistExtras.find(key) != mPersistExtras.end();
            if (key != gAgentID && !known && storedEntryCount() >= ALR_MAX_ENTRIES)
            {
                if (overflow)
                {
                    (*overflow)[key] = kv.second;   // file load: keep it in the file, just not in the live table
                }
                continue;   // same capacity rule as set(): a hostile file/scene cannot grow the table without bound
            }
            if (key == gAgentID)
            {
                kind = KIND_SELF;
            }
            Entry entry;
            entry.mKind   = kind;
            entry.mLabel  = utf8str_truncate(kv.second["label"].asString(), ALR_MAX_LABEL);
            entry.mParams = params;
            mEntries[key] = entry;
            mPersistExtras.erase(key);
            if (!ALLightResponse::isIdentity(entry.mParams))
            {
                sEverActive = true;
            }
            dirtyImpostors(key);
            any = true;
        }
    }
    if (any)
    {
        mDirty = true;
        ++mRevision;
        mChangeSignal();
    }
    return any;
}
