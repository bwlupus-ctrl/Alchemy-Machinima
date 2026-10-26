/**
 * @file alenvintensitypresets.cpp
 * @brief [EnvIntensity userpresets] Save / Load / Delete presets for the Personal
 *        Lighting "Light Intensity" strip. See alenvintensitypresets.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alenvintensitypresets.h"
#include "alsettingsundohistory.h"

#include "lldir.h"
#include "lldiriterator.h"
#include "llfile.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llnotificationsutil.h"
#include "llstring.h"
#include "lluuid.h"
#include "llviewercontrol.h"

#include <algorithm>
#if LL_WINDOWS
#include "llwin32headers.h"   // [EnvIntensity backup] MoveFileExW / FlushFileBuffers
#else
#include <fcntl.h>            // [EnvIntensity backup] open / fsync / flock
#include <sys/file.h>
#include <unistd.h>
#endif
#include <chrono>
#include <cmath>
#include <iterator>
#include <map>
#include <sstream>
#include <thread>

namespace ALEnvIntensityPresets
{

namespace
{
    const char* const PRESETS_FILENAME = "light_intensity_presets.xml";
    const S32         PRESET_VERSION   = 1;
    // A presets file bigger than this is not ours (200 presets are ~0.5 MB at most).
    const S64         MAX_FILE_BYTES   = static_cast<S64>(4) * 1024 * 1024;
    const F32         PRESET_EPS(1.e-4f);

    const char* const KEY_MOON_LINKED = "AlchemyEnvMoonLinked";

    // ---- the 17 owned settings -------------------------------------------------
    // Ranges are the Light Intensity slider ranges (floater_adjust_environment.xml),
    // i.e. what the UI can produce and the renderer's own clamps accept.
    struct FloatField
    {
        const char* mKey;
        F32 Values::* mMember;
        F64         mMin;
        F64         mMax;
    };
    const FloatField FLOAT_FIELDS[] =
    {
        { "AlchemyEnvSunEV",              &Values::mSunEV,               -4.0,     6.0 },
        { "AlchemyEnvSkyGIEV",            &Values::mSkyGIEV,             -4.0,     4.0 },
        { "AlchemyEnvMoonEV",             &Values::mMoonEV,              -4.0,     6.0 },
        { "AlchemyEnvGIAmbientEV",        &Values::mGIAmbientEV,         -3.0,     3.0 },
        { "AlchemyEnvGIProbeDiffuseEV",   &Values::mGIProbeDiffuseEV,    -3.0,     3.0 },
        { "AlchemyEnvGIProbeSpecEV",      &Values::mGIProbeSpecEV,       -3.0,     3.0 },
        { "AlchemyEnvLocalLightEV",       &Values::mLocalLightEV,        -4.0,     4.0 },
        { "AlchemyEnvLocalLightSize",     &Values::mLocalLightSize,       0.0,     0.5 },
        { "AlchemyEnvSunKelvin",          &Values::mSunKelvin,         2000.0, 12000.0 },
        { "AlchemyEnvSunTintStrength",    &Values::mSunTintStrength,      0.0,     1.0 },
        { "AlchemyEnvMoonTintStrength",   &Values::mMoonTintStrength,     0.0,     1.0 },
        { "AlchemyEnvAmbientTintStrength",&Values::mAmbientTintStrength,  0.0,     1.0 },
        { "AlchemyEnvShadowLiftEV",       &Values::mShadowLiftEV,         0.0,     2.0 },
    };

    struct ColorField
    {
        const char*    mKey;
        LLColor4 Values::* mMember;
    };
    const ColorField COLOR_FIELDS[] =
    {
        { "AlchemyEnvSunTintColor",     &Values::mSunTintColor },
        { "AlchemyEnvMoonTintColor",    &Values::mMoonTintColor },
        { "AlchemyEnvAmbientTintColor", &Values::mAmbientTintColor },
    };

    F32 defaultF32(const char* key, F32 fallback)
    {
        LLControlVariablePtr ctrl = gSavedSettings.getControl(key);
        return ctrl ? static_cast<F32>(ctrl->getDefault().asReal()) : fallback;
    }

    // ---- name helpers -------------------------------------------------------------
    std::string lowerCopy(const std::string& s)
    {
        std::string out(s);
        LLStringUtil::toLower(out);
        return out;
    }

    // lower-case, '_' and '-' -> ' ' so "golden_hour", "Golden Hour" and
    // "High-Key Studio" / "high_key_studio" all normalise alike
    std::string normalizeName(const std::string& s)
    {
        std::string out = lowerCopy(s);
        for (char& c : out)
        {
            if (c == '_' || c == '-')
            {
                c = ' ';
            }
        }
        return out;
    }

    // These names were valid user names before the Natural family was added.
    // Keep existing disk entries loadable; checkName still rejects new saves.
    bool isNaturalReservedName(const std::string& name)
    {
        static const char* const RESERVED[] =
        {
            "Natural",
            "Natural Day",
            "Natural Overcast",
            "Natural Golden Hour",
            "Natural Interior Day",
            "Hard Sun",
            "Rainy Day",
            "Natural Moonlight",
            "Rainy Night",
            "Night Interior / Neon",
        };
        const std::string norm = normalizeName(name);
        for (const char* r : RESERVED)
        {
            if (norm == normalizeName(r))
            {
                return true;
            }
        }
        return false;
    }

    // Built-in Quick Preset labels (floater_adjust_environment.xml combo
    // env_intensity_preset) + the combo's own "Custom" and section header. The
    // keys ("golden_hour") normalise onto the same strings. A new built-in preset
    // must be added here too.
    bool isReservedName(const std::string& name)
    {
        if (isNaturalReservedName(name))
        {
            return true;
        }
        static const char* const RESERVED[] =
        {
            "Stock", "Golden Hour", "Blue Hour", "Overcast Soft", "Noon Punch",
            "High-Key Studio", "Low-Key Noir", "Moonlit Night", "Neon Night",
            "Candlelit Interior", "Desert Heat", "Arctic Cold", "Soft Fill",
            "Reflections Pop", "Custom", "My presets",
        };
        const std::string norm = normalizeName(name);
        for (const char* r : RESERVED)
        {
            if (norm == normalizeName(r))
            {
                return true;
            }
        }
        return false;
    }

    // ---- store ------------------------------------------------------------------
    struct Preset
    {
        std::string mName;      // display name (as saved)
        Values      mValues;
    };
    typedef std::map<std::string, Preset> store_t;   // key = lower-case name -> sorted case-insensitively

    store_t& store()
    {
        static store_t sStore;
        return sStore;
    }

    bool sLoaded   = false;
    bool sApplying = false;
    bool sReadOnly = false;   // [EnvIntensity backup] damaged file could not be preserved: no save/remove this session

    signal_t& listChangedSignal()
    {
        static signal_t sSignal;
        return sSignal;
    }
    signal_t& appliedSignal()
    {
        static signal_t sSignal;
        return sSignal;
    }

    std::string presetsPath()
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_FILENAME);
    }

    // Clamp every value into its valid range; non-finite -> the setting default.
    // Used on save so that whatever is written always passes the loader.
    void sanitize(Values& v)
    {
        const Values defaults = defaultValues();
        for (const FloatField& f : FLOAT_FIELDS)
        {
            const F32 cur = v.*(f.mMember);
            if (!std::isfinite(cur))
            {
                v.*(f.mMember) = defaults.*(f.mMember);
            }
            else
            {
                v.*(f.mMember) = static_cast<F32>(llclamp(static_cast<F64>(cur), f.mMin, f.mMax));
            }
        }
        for (const ColorField& f : COLOR_FIELDS)
        {
            LLColor4& c = v.*(f.mMember);
            for (S32 i = 0; i < 4; ++i)
            {
                c.mV[i] = std::isfinite(c.mV[i]) ? llclamp(c.mV[i], 0.f, 1.f) : 1.f;
            }
        }
    }

    LLSD toLLSD(const Values& v)
    {
        LLSD e = LLSD::emptyMap();
        e["version"] = PRESET_VERSION;
        for (const FloatField& f : FLOAT_FIELDS)
        {
            e[f.mKey] = static_cast<F64>(v.*(f.mMember));
        }
        e[KEY_MOON_LINKED] = v.mMoonLinked;
        for (const ColorField& f : COLOR_FIELDS)
        {
            const LLColor4& c = v.*(f.mMember);
            LLSD arr = LLSD::emptyArray();
            for (S32 i = 0; i < 4; ++i)
            {
                arr.append(LLSD(static_cast<F64>(c.mV[i])));
            }
            e[f.mKey] = arr;
        }
        return e;
    }

    // Untrusted input. false = reject the whole preset (bad version / wrong type /
    // non-finite); out-of-range numbers are clamped; a missing key keeps its default.
    bool fromLLSD(const LLSD& entry, Values& out)
    {
        if (!entry.isMap())
        {
            return false;
        }
        const LLSD& version = entry["version"];
        if (!version.isInteger() || version.asInteger() != PRESET_VERSION)
        {
            return false;
        }

        Values v = defaultValues();
        for (const FloatField& f : FLOAT_FIELDS)
        {
            if (!entry.has(f.mKey))
            {
                continue;
            }
            const LLSD& s = entry[f.mKey];
            if (!s.isReal() && !s.isInteger())
            {
                return false;
            }
            F64 x = s.asReal();
            if (!std::isfinite(x))
            {
                return false;
            }
            x = llclamp(x, f.mMin, f.mMax);
            v.*(f.mMember) = static_cast<F32>(x);
        }
        if (entry.has(KEY_MOON_LINKED))
        {
            const LLSD& s = entry[KEY_MOON_LINKED];
            if (!s.isBoolean())
            {
                return false;
            }
            v.mMoonLinked = s.asBoolean();
        }
        for (const ColorField& f : COLOR_FIELDS)
        {
            if (!entry.has(f.mKey))
            {
                continue;
            }
            const LLSD& s = entry[f.mKey];
            if (!s.isArray() || s.size() != 4)
            {
                return false;
            }
            LLColor4 c;
            for (S32 i = 0; i < 4; ++i)
            {
                const LLSD& ch = s[i];
                if (!ch.isReal() && !ch.isInteger())
                {
                    return false;
                }
                const F64 x = ch.asReal();
                if (!std::isfinite(x))
                {
                    return false;
                }
                c.mV[i] = static_cast<F32>(llclamp(x, 0.0, 1.0));
            }
            v.*(f.mMember) = c;
        }
        out = v;
        return true;
    }

    // [EnvIntensity backup] Fixed names next to the presets file. Constants only: no user
    // text, file content or preset name ever forms part of a path.
    //   .prev.xml  = rolling previous version, overwritten on every replacement
    //   .bad*.xml  = damage backups (see BACKUP_FILENAMES), never overwritten
    const char* const PREVIOUS_FILENAME = "light_intensity_presets.prev.xml";

    // [EnvIntensity backup] Size + last-write time of the presets file, recorded at load
    // and after our own writes, so a change by another viewer / program is noticed.
    struct FileSig
    {
        bool mExists = false;
        S64  mSize   = 0;
        S64  mTime   = 0;
        bool operator==(const FileSig& o) const
        {
            return mExists == o.mExists && mSize == o.mSize && mTime == o.mTime;
        }
        bool operator!=(const FileSig& o) const { return !(*this == o); }
    };
    FileSig sLoadedSig;

    FileSig fileSig(const std::string& path)
    {
        FileSig sig;
#if LL_WINDOWS
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExW(ll_convert_string_to_wide(path).c_str(), GetFileExInfoStandard, &data))
        {
            sig.mExists = true;
            sig.mSize   = (static_cast<S64>(data.nFileSizeHigh) << 32) | static_cast<S64>(data.nFileSizeLow);
            sig.mTime   = (static_cast<S64>(data.ftLastWriteTime.dwHighDateTime) << 32)
                        | static_cast<S64>(data.ftLastWriteTime.dwLowDateTime);
        }
#else
        llstat st;
        if (LLFile::stat(path, &st) == 0)
        {
            sig.mExists = true;
            sig.mSize   = static_cast<S64>(st.st_size);
            sig.mTime   = static_cast<S64>(st.st_mtime);
        }
#endif
        return sig;
    }

    // [EnvIntensity backup] Defensive check before any rename: both paths must be
    // directly inside the same directory as presetsPath(), must differ, and one of
    // them must BE presetsPath() (source for a backup / previous-version move; call
    // it as (presets, other) for the restore too).
    bool isSafeBackupPair(const std::string& source, const std::string& target)
    {
        const std::string home = presetsPath();
        const std::string dir  = gDirUtilp->getDirName(home);
        return !dir.empty()
            && source == home
            && target != home
            && gDirUtilp->getDirName(source) == dir
            && gDirUtilp->getDirName(target) == dir;
    }

    // [EnvIntensity backup] Rename that can NEVER replace an existing file: on Windows
    // MoveFileExW without MOVEFILE_REPLACE_EXISTING fails atomically if the target
    // exists, closing the gap between the exists() check and the rename.
    // Elsewhere there is no such primitive here, so it simply refuses (the caller then
    // goes read-only, the safe failure) rather than racing exists()+rename.
    bool renameNoReplace(const std::string& source, const std::string& target)
    {
#if LL_WINDOWS
        return MoveFileExW(ll_convert_string_to_wide(source).c_str(),
                           ll_convert_string_to_wide(target).c_str(), 0) != 0;
#else
        (void)source;
        (void)target;
        return false;
#endif
    }

    // [EnvIntensity backup] Rename that replaces the target and (Windows) does not
    // return until the move is on disk. Only used for our own temp -> presets file and
    // presets file -> .prev slot.
    bool moveReplace(const std::string& source, const std::string& target)
    {
#if LL_WINDOWS
        return MoveFileExW(ll_convert_string_to_wide(source).c_str(),
                           ll_convert_string_to_wide(target).c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        return LLFile::rename(source, target) == 0;
#endif
    }

    // [EnvIntensity backup] Forces the (already closed) file's data to disk.
    bool flushToDisk(const std::string& path)
    {
#if LL_WINDOWS
        HANDLE h = CreateFileW(ll_convert_string_to_wide(path).c_str(), GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        const bool ok = FlushFileBuffers(h) != 0;
        CloseHandle(h);
        return ok;
#else
        const int fd = ::open(path.c_str(), O_RDWR);
        if (fd < 0)
        {
            return false;
        }
        const bool ok = ::fsync(fd) == 0;
        ::close(fd);
        return ok;
#endif
    }

    // [EnvIntensity backup] Cross-process lock around every whole transaction (reload +
    // read-modify + keep-previous + replace, and the load-time restore / damage backup /
    // salvage write), so two viewers can never interleave them. Fixed file name next to the
    // presets file; created on first use and NEVER deleted (tiny, fixed name). Held for the
    // transaction only. Re-entrant within the process (depth counter). If it cannot be taken
    // within ~2 s the operation is refused ("busy"); nothing is written.
    const char* const LOCK_FILENAME = "light_intensity_presets.lock";
    const S32         LOCK_ATTEMPTS = 40;   // x 50 ms = ~2 s
    S32               sLockDepth    = 0;
    bool              sLastBusy     = false;
#if LL_WINDOWS
    HANDLE            sLockHandle   = INVALID_HANDLE_VALUE;
#else
    int               sLockFd       = -1;
#endif

    bool acquirePresetsLock()
    {
        if (sLockDepth > 0)
        {
            ++sLockDepth;
            return true;
        }
        const std::string lock_path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, LOCK_FILENAME);
        if (!isSafeBackupPair(presetsPath(), lock_path))   // same-directory validation
        {
            return false;
        }
        for (S32 attempt = 0; attempt < LOCK_ATTEMPTS; ++attempt)
        {
            if (attempt > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
#if LL_WINDOWS
            HANDLE h = CreateFileW(ll_convert_string_to_wide(lock_path).c_str(),
                                   GENERIC_READ | GENERIC_WRITE, 0 /* share mode 0 = exclusive */,
                                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                sLockHandle = h;
                sLockDepth  = 1;
                return true;
            }
#else
            const int fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT, 0644);
            if (fd >= 0)
            {
                if (::flock(fd, LOCK_EX | LOCK_NB) == 0)
                {
                    sLockFd    = fd;
                    sLockDepth = 1;
                    return true;
                }
                ::close(fd);
            }
#endif
        }
        LL_WARNS("EnvIntensity") << "[EnvIntensity backup] presets file is busy (lock \"" << lock_path
            << "\" not available)" << LL_ENDL;
        return false;
    }

    void releasePresetsLock()
    {
        if (--sLockDepth > 0)
        {
            return;
        }
        sLockDepth = 0;
#if LL_WINDOWS
        if (sLockHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(sLockHandle);   // the file itself stays
            sLockHandle = INVALID_HANDLE_VALUE;
        }
#else
        if (sLockFd >= 0)
        {
            ::flock(sLockFd, LOCK_UN);
            ::close(sLockFd);
            sLockFd = -1;
        }
#endif
    }

    struct PresetsLock
    {
        PresetsLock() : mHeld(acquirePresetsLock()) {}
        ~PresetsLock() { if (mHeld) { releasePresetsLock(); } }
        PresetsLock(const PresetsLock&) = delete;
        PresetsLock& operator=(const PresetsLock&) = delete;
        bool held() const { return mHeld; }
    private:
        bool mHeld;
    };

    // Crash-safe write: temp -> flush -> close -> fsync -> (previous version kept) ->
    // replace, all checked; a failure removes the temp file and leaves the existing
    // file untouched. [EnvIntensity backup] The current file is first moved to the
    // fixed .prev slot, so every replacement leaves the previous bytes recoverable; if
    // that cannot be done the write is refused (a missing file needs no previous).
    bool writeAtomic(const std::string& path, const LLSD& content)
    {
        const std::string temporary = path + "." + LLUUID::generateNewID().asString() + ".tmp";
        llofstream output(temporary.c_str());
        if (!output.is_open())
        {
            return false;
        }
        const S32 serialized = LLSDSerialize::toPrettyXML(content, output);
        output.flush();
        const bool succeeded = serialized > 0 && output.good();
        output.close();
        if (!succeeded || output.fail() || !flushToDisk(temporary))
        {
            LLFile::remove(temporary);
            return false;
        }

        bool moved_previous = false;
        const std::string previous = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PREVIOUS_FILENAME);
        if (LLFile::exists(path))
        {
            if (!isSafeBackupPair(path, previous) || !moveReplace(path, previous))
            {
                LL_WARNS("EnvIntensity") << "[EnvIntensity backup] could not keep the previous version as \""
                    << previous << "\"; save refused" << LL_ENDL;
                LLFile::remove(temporary);
                return false;
            }
            moved_previous = true;
        }
        if (!moveReplace(temporary, path))
        {
            if (moved_previous && isSafeBackupPair(path, previous))
            {
                renameNoReplace(previous, path);   // put the old file back (P is absent, so this cannot clobber)
            }
            LLFile::remove(temporary);
            return false;
        }
        return true;
    }

    bool writeStore()
    {
        if (sLockDepth <= 0)
        {
            return false;   // [EnvIntensity backup] never write outside the cross-process lock
        }
        LLSD root = LLSD::emptyMap();
        for (const auto& kv : store())
        {
            root[kv.second.mName] = toLLSD(kv.second.mValues);
        }
        const std::string path = presetsPath();
        if (!writeAtomic(path, root))
        {
            return false;
        }
        sLoadedSig = fileSig(path);   // our own write is not "another viewer's change"
        return true;
    }

    // [EnvIntensity backup] Fixed backup file names, tried in order; the first one
    // that does not exist wins and an existing one is never overwritten.
    const char* const BACKUP_FILENAMES[] =
    {
        "light_intensity_presets.bad.xml",
        "light_intensity_presets.bad.2.xml",
        "light_intensity_presets.bad.3.xml",
        "light_intensity_presets.bad.4.xml",
        "light_intensity_presets.bad.5.xml",
    };

    // [EnvIntensity backup] Preserves a damaged presets file by RENAMING it (every byte
    // kept, no copy, works for oversized files). Returns the backup file NAME on
    // success; empty = impossible (all names taken, guard refused, or the rename failed)
    // in which case nothing at all has been changed.
    std::string backupDamagedFile(const std::string& source)
    {
        for (const char* const name : BACKUP_FILENAMES)
        {
            const std::string target = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, name);
            if (LLFile::exists(target))   // file OR directory: never overwrite
            {
                continue;
            }
            if (!isSafeBackupPair(source, target))
            {
                LL_WARNS("EnvIntensity") << "[EnvIntensity backup] refusing to rename \"" << source
                    << "\" to \"" << target << "\": not in the presets directory" << LL_ENDL;
                return std::string();
            }
            if (!renameNoReplace(source, target))
            {
                LL_WARNS("EnvIntensity") << "[EnvIntensity backup] could not rename \"" << source
                    << "\" to \"" << target << "\"" << LL_ENDL;
                return std::string();
            }
            return name;
        }
        LL_WARNS("EnvIntensity") << "[EnvIntensity backup] all " << std::size(BACKUP_FILENAMES)
            << " backup names already exist next to \"" << source << "\"" << LL_ENDL;
        return std::string();
    }

    // [EnvIntensity backup] Called once, after a load that found damage. `damage`
    // describes it (for the log); the store holds whatever could be salvaged.
    void handleDamagedFile(const std::string& path, const std::string& damage)
    {
        const S32 salvaged = static_cast<S32>(store().size());
        LL_WARNS("EnvIntensity") << "[EnvIntensity backup] presets file \"" << path << "\" is damaged ("
            << damage << "); " << salvaged << " preset(s) recovered" << LL_ENDL;

        const std::string backup = backupDamagedFile(path);
        if (backup.empty())
        {
            // Nothing was renamed, rewritten or deleted: the damaged file is exactly as
            // it was. Refuse every save/remove so it can never be replaced.
            sReadOnly = true;
            LL_WARNS("EnvIntensity") << "[EnvIntensity backup] read-only for this session; "
                << "saving and deleting presets is disabled" << LL_ENDL;
            LLNotificationsUtil::add("EnvIntensityPresetsDamagedReadOnly",
                LLSD().with("COUNT", salvaged));
            return;
        }

        LL_WARNS("EnvIntensity") << "[EnvIntensity backup] original kept as \"" << backup << "\"" << LL_ENDL;
        if (salvaged > 0 && !writeStore())
        {
            // The original is safe in the backup; the next save retries the write.
            LL_WARNS("EnvIntensity") << "[EnvIntensity backup] could not write the recovered presets back" << LL_ENDL;
        }
        LLNotificationsUtil::add("EnvIntensityPresetsDamaged",
            LLSD().with("FILENAME", backup).with("COUNT", salvaged));
    }

    // [EnvIntensity backup] Hostile-input limits, enforced BEFORE the XML parser sees the file.
    const S32 MAX_CONTAINER_DEPTH = 6;   // our own files nest 3 deep (root map > preset map > color array)

    bool isAllowedElement(const std::string& name)
    {
        static const char* const ALLOWED[] =
        {
            "llsd", "map", "array", "key", "real", "integer", "boolean", "string", "undef",
        };
        for (const char* a : ALLOWED)
        {
            if (name == a)
            {
                return true;
            }
        }
        return false;
    }

    // Pre-scan of the raw XML text. false = reject as damaged (reason in `why`):
    // more than MAX_CONTAINER_DEPTH nested <map>/<array> (self-closing ones included),
    // an element name outside the LLSD subset we write, a DOCTYPE / entity declaration,
    // or malformed markup.
    bool prescanXml(const std::string& text, std::string& why)
    {
        const size_t n = text.size();
        size_t i = 0;
        S32 depth = 0;
        while (i < n)
        {
            i = text.find('<', i);
            if (i == std::string::npos)
            {
                break;
            }
            if (text.compare(i, 4, "<!--") == 0)
            {
                const size_t e = text.find("-->", i + 4);
                if (e == std::string::npos) { why = "unterminated comment"; return false; }
                i = e + 3;
                continue;
            }
            if (text.compare(i, 9, "<![CDATA[") == 0)
            {
                const size_t e = text.find("]]>", i + 9);
                if (e == std::string::npos) { why = "unterminated CDATA"; return false; }
                i = e + 3;
                continue;
            }
            if (text.compare(i, 2, "<?") == 0)
            {
                const size_t e = text.find("?>", i + 2);
                if (e == std::string::npos) { why = "unterminated declaration"; return false; }
                i = e + 2;
                continue;
            }
            if (text.compare(i, 2, "<!") == 0)
            {
                why = "DOCTYPE / entity declaration not allowed";
                return false;
            }

            // an element tag: find its closing '>' (quoted attribute values may contain '>')
            size_t j = i + 1;
            char quote = 0;
            while (j < n && (quote != 0 || text[j] != '>'))
            {
                if (quote != 0)
                {
                    if (text[j] == quote) { quote = 0; }
                }
                else if (text[j] == '"' || text[j] == '\'')
                {
                    quote = text[j];
                }
                ++j;
            }
            if (j >= n)
            {
                why = "unterminated tag";
                return false;
            }
            const bool closing      = (text[i + 1] == '/');
            const bool self_closing = (text[j - 1] == '/');
            size_t name_start = i + 1 + (closing ? 1 : 0);
            size_t name_end   = name_start;
            while (name_end < j && text[name_end] != ' ' && text[name_end] != '\t'
                   && text[name_end] != '\r' && text[name_end] != '\n' && text[name_end] != '/')
            {
                ++name_end;
            }
            const std::string name = text.substr(name_start, name_end - name_start);
            if (!isAllowedElement(name))
            {
                why = "unexpected element <" + name.substr(0, 32) + ">";
                return false;
            }
            if (name == "map" || name == "array")
            {
                if (closing)
                {
                    if (--depth < 0) { why = "unbalanced container tags"; return false; }
                }
                else
                {
                    if (++depth > MAX_CONTAINER_DEPTH) { why = "container nesting too deep"; return false; }
                    if (self_closing) { --depth; }
                }
            }
            i = j + 1;
        }
        return true;
    }

    // [EnvIntensity backup] Reads the whole file, enforcing the size cap on the bytes
    // actually read (not just the size reported up front).
    bool readCapped(const std::string& path, std::string& out, std::string& why)
    {
        llifstream input(path, std::ios::in | std::ios::binary);
        if (!input.is_open())
        {
            why = "cannot be opened for reading";
            return false;
        }
        char buffer[65536];
        for (;;)
        {
            input.read(buffer, sizeof(buffer));
            const std::streamsize got = input.gcount();
            if (got > 0)
            {
                out.append(buffer, static_cast<size_t>(got));
                if (static_cast<S64>(out.size()) > MAX_FILE_BYTES)
                {
                    why = "more than " + std::to_string(MAX_FILE_BYTES) + " bytes";
                    return false;
                }
            }
            if (!input)
            {
                break;
            }
        }
        const bool read_error = input.bad();
        input.close();   // must be closed before any rename
        if (read_error)
        {
            why = "read error";
            return false;
        }
        return true;
    }

    // [EnvIntensity backup] Leftover temp files from an interrupted write. Only names of the
    // exact form "light_intensity_presets.xml.<36-char UUID>.tmp" are counted. They are only
    // logged: never deleted.
    void logStrayTempFiles()
    {
        const std::string prefix = std::string(PRESETS_FILENAME) + ".";
        const std::string suffix = ".tmp";
        const std::string dir    = gDirUtilp->getDirName(presetsPath());
        if (dir.empty())
        {
            return;
        }
        S32 count = 0;
        LLDirIterator iter(dir, prefix + "*" + suffix);
        std::string name;
        while (iter.next(name))
        {
            if (name.size() == prefix.size() + 36 + suffix.size()
                && name.compare(0, prefix.size(), prefix) == 0
                && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
            {
                ++count;
            }
        }
        if (count > 0)
        {
            LL_WARNS("EnvIntensity") << "[EnvIntensity backup] " << count
                << " leftover temp file(s) next to the presets file (left in place)" << LL_ENDL;
        }
    }

    void loadFromDisk()
    {
        sLoaded = true;
        sReadOnly = false;
        store().clear();

        // [EnvIntensity backup] The restore / damage backup / salvage write below are
        // transactions: hold the cross-process lock. If it is busy we still READ the file
        // for display but change nothing on disk, and mark the snapshot "unknown" so the
        // next save/remove reloads it (under the lock) before writing.
        PresetsLock lock;
        const bool have_lock = lock.held();

        const std::string path = presetsPath();
        logStrayTempFiles();

        // [EnvIntensity backup] Crash between "P -> .prev" and "temp -> P" leaves P missing
        // with the data in .prev: restore it (never replacing anything) and load it normally.
        // If that cannot be done, load nothing and go read-only so .prev is never replaced.
        if (have_lock && !LLFile::exists(path))
        {
            const std::string previous = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PREVIOUS_FILENAME);
            if (LLFile::isfile(previous))
            {
                if (isSafeBackupPair(path, previous) && renameNoReplace(previous, path))
                {
                    LL_WARNS("EnvIntensity") << "[EnvIntensity backup] presets file was missing; restored from \""
                        << previous << "\"" << LL_ENDL;
                    LLNotificationsUtil::add("EnvIntensityPresetsRestored");
                }
                else
                {
                    sReadOnly = true;
                    LL_WARNS("EnvIntensity") << "[EnvIntensity backup] presets file missing and \"" << previous
                        << "\" could not be restored; read-only for this session" << LL_ENDL;
                    sLoadedSig = fileSig(path);
                    LLNotificationsUtil::add("EnvIntensityPresetReadOnly");
                    return;
                }
            }
        }

        const FileSig before = fileSig(path);   // taken BEFORE reading: a change during the read is seen as stale later
        sLoadedSig = before;
        if (!have_lock)
        {
            sLoadedSig.mSize = -1;   // never equals a real signature: next write reloads under the lock
        }
        if (!LLFile::exists(path))
        {
            return;   // first run (or nothing salvageable was left): nothing saved yet
        }

        // Empty = the file is fine. Anything else is the reason it is damaged.
        std::string damage;
        const S64 bytes = LLFile::isfile(path) ? LLFile::size(path) : -1;
        std::string text;
        if (bytes < 0 || bytes > MAX_FILE_BYTES)
        {
            damage = "not a regular file, or size " + std::to_string(bytes) + " bytes is out of range";
        }
        else if (readCapped(path, text, damage) && prescanXml(text, damage))
        {
            std::istringstream input(text);
            LLSD root;
            const S32 ret = LLSDSerialize::fromXML(root, input);
            if (ret == LLSDParser::PARSE_FAILURE || !root.isMap())
            {
                damage = "unreadable or not a preset map";
            }
            else
            {
                S32 rejected = 0;
                for (LLSD::map_const_iterator it = root.beginMap(); it != root.endMap(); ++it)
                {
                    std::string clean;
                    const NameCheck name_check = checkName(it->first, clean);
                    if ((name_check != NAME_OK &&
                         !(name_check == NAME_RESERVED && isNaturalReservedName(clean))) || clean != it->first)
                    {
                        ++rejected;
                        continue;
                    }
                    const std::string key = lowerCopy(clean);
                    if (store().size() >= MAX_PRESETS || store().find(key) != store().end())
                    {
                        ++rejected;
                        continue;
                    }
                    Preset p;
                    p.mName = clean;
                    if (!fromLLSD(it->second, p.mValues))
                    {
                        ++rejected;
                        continue;
                    }
                    store()[key] = p;
                }
                if (rejected > 0)
                {
                    damage = std::to_string(rejected) + " invalid preset(s) rejected";
                }
            }
        }
        // (readCapped / prescanXml failures leave `damage` set; the store is still empty)

        if (!damage.empty())
        {
            if (have_lock)
            {
                handleDamagedFile(path, damage);
                sLoadedSig = fileSig(path);   // whatever the backup / write-back left behind is ours
            }
            else
            {
                LL_WARNS("EnvIntensity") << "[EnvIntensity backup] presets file looks damaged (" << damage
                    << ") but is busy; backup deferred to the next save" << LL_ENDL;
            }
        }
    }

    void ensureLoaded()
    {
        if (!sLoaded)
        {
            loadFromDisk();
        }
    }

    // [EnvIntensity backup] Never write from a stale snapshot: if the presets file changed
    // on disk since we loaded (or last wrote) it, reload it through the full damage-check
    // path first. Returns true if the store was reloaded.
    bool reloadIfChanged()
    {
        // Always reload under the transaction lock (the file is tiny): a size+mtime
        // signature can miss two same-length writes within one timestamp tick (POSIX
        // whole-second mtime), which would let a stale store overwrite another
        // viewer's save. The return value only reports whether the signature moved.
        const bool changed = fileSig(presetsPath()) != sLoadedSig;
        if (changed)
        {
            LL_WARNS("EnvIntensity") << "[EnvIntensity backup] presets file changed on disk since it was loaded; reloading" << LL_ENDL;
        }
        loadFromDisk();
        return changed;
    }

    struct ApplyGuard
    {
        ApplyGuard() : mPrev(sApplying) { sApplying = true; }
        ~ApplyGuard() { sApplying = mPrev; }
        bool mPrev;
    };

    inline bool nearlyEqualF(F32 a, F32 b)
    {
        return std::fabs(a - b) < PRESET_EPS;
    }
    inline bool nearlyEqualColor(const LLColor4& a, const LLColor4& b)
    {
        return nearlyEqualF(a.mV[0], b.mV[0]) && nearlyEqualF(a.mV[1], b.mV[1])
            && nearlyEqualF(a.mV[2], b.mV[2]) && nearlyEqualF(a.mV[3], b.mV[3]);
    }
}   // anonymous namespace

//-------------------------------------------------------------------------
Values currentValues()
{
    Values v;
    v.mSunEV               = gSavedSettings.getF32("AlchemyEnvSunEV");
    v.mSkyGIEV             = gSavedSettings.getF32("AlchemyEnvSkyGIEV");
    v.mMoonLinked          = gSavedSettings.getBOOL("AlchemyEnvMoonLinked");
    v.mMoonEV              = gSavedSettings.getF32("AlchemyEnvMoonEV");
    v.mGIAmbientEV         = gSavedSettings.getF32("AlchemyEnvGIAmbientEV");
    v.mGIProbeDiffuseEV    = gSavedSettings.getF32("AlchemyEnvGIProbeDiffuseEV");
    v.mGIProbeSpecEV       = gSavedSettings.getF32("AlchemyEnvGIProbeSpecEV");
    v.mLocalLightEV        = gSavedSettings.getF32("AlchemyEnvLocalLightEV");
    v.mLocalLightSize      = gSavedSettings.getF32("AlchemyEnvLocalLightSize");
    v.mSunKelvin           = gSavedSettings.getF32("AlchemyEnvSunKelvin");
    v.mSunTintColor        = gSavedSettings.getColor4("AlchemyEnvSunTintColor");
    v.mSunTintStrength     = gSavedSettings.getF32("AlchemyEnvSunTintStrength");
    v.mMoonTintColor       = gSavedSettings.getColor4("AlchemyEnvMoonTintColor");
    v.mMoonTintStrength    = gSavedSettings.getF32("AlchemyEnvMoonTintStrength");
    v.mAmbientTintColor    = gSavedSettings.getColor4("AlchemyEnvAmbientTintColor");
    v.mAmbientTintStrength = gSavedSettings.getF32("AlchemyEnvAmbientTintStrength");
    v.mShadowLiftEV        = gSavedSettings.getF32("AlchemyEnvShadowLiftEV");
    return v;
}

Values defaultValues()
{
    Values v;   // member initialisers = the shipped defaults; overridden from the live defaults
    for (const FloatField& f : FLOAT_FIELDS)
    {
        v.*(f.mMember) = defaultF32(f.mKey, v.*(f.mMember));
    }
    if (LLControlVariablePtr ctrl = gSavedSettings.getControl(KEY_MOON_LINKED))
    {
        v.mMoonLinked = ctrl->getDefault().asBoolean();
    }
    for (const ColorField& f : COLOR_FIELDS)
    {
        if (LLControlVariablePtr ctrl = gSavedSettings.getControl(f.mKey))
        {
            const LLSD def = ctrl->getDefault();
            if (def.isArray() && def.size() == 4)
            {
                v.*(f.mMember) = LLColor4(def);
            }
        }
    }
    return v;
}

bool nearlyEqual(const Values& a, const Values& b)
{
    return nearlyEqualF(a.mSunEV, b.mSunEV)
        && nearlyEqualF(a.mSkyGIEV, b.mSkyGIEV)
        && a.mMoonLinked == b.mMoonLinked
        && nearlyEqualF(a.mMoonEV, b.mMoonEV)
        && nearlyEqualF(a.mGIAmbientEV, b.mGIAmbientEV)
        && nearlyEqualF(a.mGIProbeDiffuseEV, b.mGIProbeDiffuseEV)
        && nearlyEqualF(a.mGIProbeSpecEV, b.mGIProbeSpecEV)
        && nearlyEqualF(a.mLocalLightEV, b.mLocalLightEV)
        && nearlyEqualF(a.mLocalLightSize, b.mLocalLightSize)
        && nearlyEqualF(a.mSunKelvin, b.mSunKelvin)
        && nearlyEqualColor(a.mSunTintColor, b.mSunTintColor)
        && nearlyEqualF(a.mSunTintStrength, b.mSunTintStrength)
        && nearlyEqualColor(a.mMoonTintColor, b.mMoonTintColor)
        && nearlyEqualF(a.mMoonTintStrength, b.mMoonTintStrength)
        && nearlyEqualColor(a.mAmbientTintColor, b.mAmbientTintColor)
        && nearlyEqualF(a.mAmbientTintStrength, b.mAmbientTintStrength)
        && nearlyEqualF(a.mShadowLiftEV, b.mShadowLiftEV);
}

void apply(const Values& values)
{
    ALExposureSettingsHistory::ScopedBatch history_batch("Preset");
    {
        ApplyGuard guard;   // suppresses the Quick Presets combo refresh while the preset signals fire
        gSavedSettings.setF32("AlchemyEnvSunEV", values.mSunEV);
        gSavedSettings.setF32("AlchemyEnvSkyGIEV", values.mSkyGIEV);
        gSavedSettings.setBOOL("AlchemyEnvMoonLinked", values.mMoonLinked);
        gSavedSettings.setF32("AlchemyEnvMoonEV", values.mMoonEV);
        gSavedSettings.setF32("AlchemyEnvGIAmbientEV", values.mGIAmbientEV);
        gSavedSettings.setF32("AlchemyEnvGIProbeDiffuseEV", values.mGIProbeDiffuseEV);
        gSavedSettings.setF32("AlchemyEnvGIProbeSpecEV", values.mGIProbeSpecEV);
        gSavedSettings.setF32("AlchemyEnvLocalLightEV", values.mLocalLightEV);
        gSavedSettings.setF32("AlchemyEnvLocalLightSize", values.mLocalLightSize);
        gSavedSettings.setF32("AlchemyEnvSunKelvin", values.mSunKelvin);
        gSavedSettings.setColor4("AlchemyEnvSunTintColor", values.mSunTintColor);
        gSavedSettings.setF32("AlchemyEnvSunTintStrength", values.mSunTintStrength);
        gSavedSettings.setColor4("AlchemyEnvMoonTintColor", values.mMoonTintColor);
        gSavedSettings.setF32("AlchemyEnvMoonTintStrength", values.mMoonTintStrength);
        gSavedSettings.setColor4("AlchemyEnvAmbientTintColor", values.mAmbientTintColor);
        gSavedSettings.setF32("AlchemyEnvAmbientTintStrength", values.mAmbientTintStrength);
        gSavedSettings.setF32("AlchemyEnvShadowLiftEV", values.mShadowLiftEV);
    }
    appliedSignal()();
}

bool isApplying()
{
    return sApplying;
}

//-------------------------------------------------------------------------
NameCheck checkName(const std::string& raw, std::string& out_clean)
{
    out_clean = raw;
    LLStringUtil::trim(out_clean);
    if (out_clean.empty()
        || out_clean.front() == ' ' || out_clean.back() == ' '
        || out_clean.size() > MAX_NAME_CHARS * 4   // UTF-8 worst case, before the exact character count
        || utf8str_to_wstring(out_clean).size() > MAX_NAME_CHARS)
    {
        return NAME_INVALID;
    }
    for (const char c : out_clean)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7f)
        {
            return NAME_INVALID;
        }
    }
    if (isReservedName(out_clean))
    {
        return NAME_RESERVED;
    }
    return NAME_OK;
}

//-------------------------------------------------------------------------
std::vector<std::string> listNames()
{
    ensureLoaded();
    std::vector<std::string> names;
    names.reserve(store().size());
    for (const auto& kv : store())
    {
        names.push_back(kv.second.mName);
    }
    return names;
}

bool exists(const std::string& name)
{
    ensureLoaded();
    return store().find(lowerCopy(name)) != store().end();
}

bool getValues(const std::string& name, Values& out_values, std::string* out_canonical_name)
{
    ensureLoaded();
    const auto it = store().find(lowerCopy(name));
    if (it == store().end())
    {
        return false;
    }
    out_values = it->second.mValues;
    if (out_canonical_name)
    {
        *out_canonical_name = it->second.mName;
    }
    return true;
}

std::string findMatchingName(const Values& values)
{
    ensureLoaded();
    for (const auto& kv : store())
    {
        if (nearlyEqual(values, kv.second.mValues))
        {
            return kv.second.mName;
        }
    }
    return std::string();
}

SaveResult save(const std::string& raw_name, const Values& values)
{
    ensureLoaded();
    sLastBusy = false;
    std::string clean;
    const NameCheck check = checkName(raw_name, clean);
    if (check == NAME_INVALID)
    {
        return SAVE_INVALID_NAME;
    }
    if (check == NAME_RESERVED)
    {
        return SAVE_RESERVED_NAME;
    }

    // [EnvIntensity backup] The whole transaction (reload, read-modify, keep-previous,
    // replace) runs under the cross-process lock. Busy = refuse, write nothing.
    PresetsLock lock;
    if (!lock.held())
    {
        sLastBusy = true;
        return SAVE_BUSY;
    }
    if (reloadIfChanged())   // never write from a stale snapshot
    {
        listChangedSignal()();
    }
    if (sReadOnly)
    {
        return SAVE_READ_ONLY;
    }

    const std::string key = lowerCopy(clean);
    const auto found = store().find(key);
    const bool existed = (found != store().end());
    if (!existed && store().size() >= MAX_PRESETS)
    {
        return SAVE_LIMIT_REACHED;
    }
    Preset previous;
    if (existed)
    {
        previous = found->second;
    }

    Preset p;
    p.mName   = clean;
    p.mValues = values;
    sanitize(p.mValues);
    store()[key] = p;

    if (!writeStore())
    {
        if (existed)
        {
            store()[key] = previous;
        }
        else
        {
            store().erase(key);
        }
        return SAVE_WRITE_FAILED;
    }
    listChangedSignal()();
    return SAVE_OK;
}

SaveResult saveCurrent(const std::string& raw_name)
{
    return save(raw_name, currentValues());
}

bool lastWriteWasBusy()
{
    return sLastBusy;   // [EnvIntensity backup]
}

bool isReadOnly()
{
    ensureLoaded();
    return sReadOnly;   // [EnvIntensity backup]
}

bool remove(const std::string& name)
{
    ensureLoaded();
    sLastBusy = false;
    PresetsLock lock;   // [EnvIntensity backup] whole transaction under the cross-process lock
    if (!lock.held())
    {
        sLastBusy = true;
        return false;
    }
    if (reloadIfChanged())   // never write from a stale snapshot
    {
        listChangedSignal()();
    }
    if (sReadOnly)
    {
        return false;
    }
    const std::string key = lowerCopy(name);
    const auto found = store().find(key);
    if (found == store().end())
    {
        return false;
    }
    const Preset previous = found->second;
    store().erase(found);
    if (!writeStore())
    {
        store()[key] = previous;
        return false;
    }
    listChangedSignal()();
    return true;
}

bool applyNamed(const std::string& name)
{
    Values v;
    if (!getValues(name, v))
    {
        return false;
    }
    ALExposureSettingsHistory::ScopedBatch history_batch("Preset '" + name + "'");
    apply(v);
    return true;
}

//-------------------------------------------------------------------------
boost::signals2::connection connectListChanged(const signal_t::slot_type& slot)
{
    return listChangedSignal().connect(slot);
}

boost::signals2::connection connectApplied(const signal_t::slot_type& slot)
{
    return appliedSignal().connect(slot);
}

}   // namespace ALEnvIntensityPresets
