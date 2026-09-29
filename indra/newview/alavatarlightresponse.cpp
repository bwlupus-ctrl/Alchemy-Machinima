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

#include "alghoststudio.h"
#include "llagentdata.h"
#include "llappviewer.h"
#include "llcontrolavatar.h"
#include "llcharacter.h"
#include "lldir.h"
#include "llfile.h"
#include "llghostavatar.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "lltimer.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

#include <algorithm>

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
}

bool ALAvatarLightResponse::sEverActive = false;

// static
ALAvatarLightResponse& ALAvatarLightResponse::instance()
{
    static ALAvatarLightResponse sInstance;
    return sInstance;
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
    EKind kind = KIND_RESIDENT;
    LLUUID key;
    if (av)
    {
        if (av->isSelf())
        {
            kind = KIND_SELF;
            key = gAgentID;
        }
        else if (av->isGhostAvatar())
        {
            kind = KIND_CLONE;
            const ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().findInstanceByRuntime(av->getID());
            if (inst)
            {
                key = inst->mId;
            }
        }
        else if (av->isControlAvatar())
        {
            kind = KIND_ANIMESH;
            key = av->getActorFxOwnerId();
        }
        else
        {
            key = av->getID();
        }
    }
    if (out)
    {
        *out = kind;
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
    if (kind != KIND_CLONE && key != gAgentID && mEntries.find(key) == mEntries.end()
        && storedEntryCount() >= ALR_MAX_ENTRIES)
    {
        return false;
    }
    Entry entry;
    entry.mKind   = kind;
    entry.mLabel  = utf8str_truncate(label, ALR_MAX_LABEL);
    entry.mParams = ALLightResponse::sanitize(params);
    mEntries[key] = entry;
    if (!ALLightResponse::isIdentity(entry.mParams))
    {
        sEverActive = true;
    }
    markChanged(key);
    if (save_now && kind != KIND_CLONE)
    {
        saveIfPersist();
    }
    return true;
}

void ALAvatarLightResponse::inherit(const LLUUID& key, bool save_now)
{
    const auto it = mEntries.find(key);
    if (it == mEntries.end())
    {
        return;
    }
    const EKind kind = it->second.mKind;
    mEntries.erase(it);
    markChanged(key);
    if (save_now && kind != KIND_CLONE)
    {
        saveIfPersist();
    }
}

void ALAvatarLightResponse::clearAll(bool save_now)
{
    if (mEntries.empty())
    {
        return;
    }
    mEntries.clear();
    mDirty = true;
    LLVOAvatar::resetImpostors();
    mChangeSignal();
    if (save_now)
    {
        saveIfPersist();
    }
}

void ALAvatarLightResponse::markChanged(const LLUUID& key)
{
    mDirty = true;
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

    static LLCachedControl<bool> master(gSavedSettings, "AvatarLightResponseEnabled", true);
    if (!master() || mEntries.empty())
    {
        return;
    }

    for (const auto& kv : mEntries)
    {
        Slot slot;
        slot.mIdentity = ALLightResponse::isIdentity(kv.second.mParams);
        slot.mPacked   = ALLightResponse::pack(kv.second.mParams);
        slot.mStoreKey = kv.first;
        mTable[kv.first] = slot;
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

const LLVector4* ALAvatarLightResponse::lookup(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3,
                                               LLUUID* hit_key)
{
    const Slot* slot = findSlot(k1, k2, k3, hit_key);
    return (slot && !slot->mIdentity) ? &slot->mPacked : nullptr;
}

const LLVector4* ALAvatarLightResponse::peek(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3)
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
        << " live_over_budget=" << countLiveOverBudget() << LL_ENDL;

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
            << " VERDICT=" << verdict << LL_ENDL;
    }
}

void ALAvatarLightResponse::saveIfPersist() const
{
    if (gSavedSettings.getBOOL("AvatarLightResponsePersist"))
    {
        saveToFile();
    }
}

void ALAvatarLightResponse::saveToFile() const
{
    if (gAgentID.isNull())
    {
        return;   // "self" cannot be keyed yet: writing now would erase the saved self entry
    }
    LLSD entries = LLSD::emptyMap();
    size_t written = 0;
    // Own entry first so a (never expected) over-cap map still cannot drop it.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (const auto& kv : mEntries)
        {
            if (kv.second.mKind == KIND_CLONE || ((kv.first == gAgentID) != (pass == 0)))
            {
                continue;   // clone keys are session-only
            }
            if (pass == 1 && written >= ALR_MAX_ENTRIES)
            {
                break;
            }
            LLSD e = ALLightResponse::toLLSD(kv.second.mParams);
            e["kind"]  = kindToString(kv.second.mKind);
            e["label"] = kv.second.mLabel;
            entries[keyToString(kv.first)] = e;
            ++written;
        }
    }
    LLSD root = LLSD::emptyMap();
    root["version"] = 1;
    root["entries"] = entries;

    const std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, ALR_FILE_NAME);
    llofstream out(path.c_str());
    if (!out.is_open())
    {
        LL_WARNS("AvatarLightResponse") << "Can't open \"" << path << "\" for writing" << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(root, out);
    out.close();
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
    mUploadsNow.clear();
    mUploadsLast.clear();
    mDirty = true;
    mChangeSignal();

    if (!gSavedSettings.getBOOL("AvatarLightResponsePersist"))
    {
        return;
    }
    const std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, ALR_FILE_NAME);
    llifstream in(path.c_str());
    if (!in.is_open())
    {
        return;   // first run: nothing saved yet
    }
    LLSD root;
    const S32 ret = LLSDSerialize::fromXML(root, in);
    in.close();
    if (ret == LLSDParser::PARSE_FAILURE || !root.isMap())
    {
        LL_WARNS("AvatarLightResponse") << "Ignoring unreadable \"" << path << "\"" << LL_ENDL;
        return;
    }
    importMerge(root);
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
            const int priority = (key == gAgentID) ? 0 : (ALLightResponse::isIdentity(params) ? 1 : 2);
            if (priority != pass)
            {
                continue;
            }
            if (key != gAgentID && mEntries.find(key) == mEntries.end() && storedEntryCount() >= ALR_MAX_ENTRIES)
            {
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
        mChangeSignal();
    }
    return any;
}
