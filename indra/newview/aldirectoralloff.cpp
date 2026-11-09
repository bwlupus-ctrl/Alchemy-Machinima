#include "llviewerprecompiledheaders.h"
#include "aldirectoralloff.h"

#include "alavatarlightresponse.h"
#include "alcinelightrigmanager.h"
#include "alghoststudio.h"
#include "aldirectorswitcher.h"
#include "alsettingsundohistory.h"
#include "llactormover.h"
#include "llagent.h"
#include "llapp.h"
#include "lldirectorcast.h"
#include "lldir.h"
#include "llfile.h"
#include "llflycamrecorder.h"
#include "llghostavatar.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"

#include <set>
#include <sstream>

namespace ALDirectorAllOff
{
bool cameraTakeActive()
{
    const auto& recorder = LLFlycamRecorder::instance();
    return recorder.getState() == LLFlycamRecorder::STATE_RECORDING || recorder.isPlaybackActive();
}

namespace
{
std::string backupPath()
{
    return gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "director_last_all_off.xml");
}

bool idle(std::string& message)
{
    auto& cast = LLDirectorCast::instance();
    if (cast.isCountingDown() || cast.isRunning())
    {
        message = "CUT the take before resetting or restoring Director features";
        return false;
    }
    if (cameraTakeActive())
    {
        message = "Stop the camera take before resetting or restoring Director features";
        return false;
    }
    if (gAgentID.isNull() || gDirUtilp->getLindenUserDir().empty())
    {
        message = "Log in before resetting or restoring Director features";
        return false;
    }
    return true;
}

bool sessionActor(const LLUUID& id)
{
    const auto& studio = ALGhostStudio::instance();
    return studio.getInstance(id) || studio.findInstanceByRuntime(id) ||
        studio.isPrivateOverlayRuntime(id) || LLGhostAvatar::isGhostId(id);
}

LLSD actors()
{
    auto& cast = LLDirectorCast::instance();
    auto& mover = LLActorMover::instance();
    std::set<LLUUID> ids{LLUUID::null};
    for (const auto& actor : cast.getCast()) ids.insert(cast.runtimeActorId(actor.mId));
    for (const auto& actor : mover.gazeActors()) ids.insert(cast.runtimeActorId(actor));
    LLSD result = LLSD::emptyArray();
    for (const LLUUID& id : ids)
    {
        if (sessionActor(id)) continue;
        LLSD actor;
        actor["id"] = id;
        actor["fx"] = cast.getActorStyle(id).mEnabled;
        actor["gaze"] = mover.isGazeEnabled(id);
        result.append(actor);
    }
    return result;
}

void applyActors(const LLSD& data, bool restoring, S32& skipped)
{
    auto& cast = LLDirectorCast::instance();
    auto& mover = LLActorMover::instance();
    for (const LLSD& actor : llsd::inArray(data))
    {
        const LLUUID id = actor["id"].asUUID();
        // Retained cast records are editable while away, exactly like the page.
        // Never re-create a removed actor (or edit a session clone).
        if (sessionActor(id) || (restoring && id.notNull() && !cast.containsStored(id) && !cast.resolve(id)))
        {
            if (restoring) ++skipped;
            continue;
        }
        auto style = cast.getActorStyle(id);
        const bool fx = restoring && actor["fx"].asBoolean();
        if (style.mEnabled != fx)
        {
            style.mEnabled = fx;
            cast.setActorStyle(id, style);
        }
        const bool gaze = restoring && actor["gaze"].asBoolean();
        if (mover.isGazeEnabled(id) != gaze) mover.setGazeEnabled(id, gaze);
    }
}

bool readBackup(LLSD& root, std::string& reason)
{
    if (gAgentID.isNull() || gDirUtilp->getLindenUserDir().empty())
    {
        reason = "Log in to restore the last reset";
        return false;
    }
    llifstream in(backupPath().c_str());
    if (!in.is_open())
    {
        reason = "No ALL OFF backup for this account yet";
        return false;
    }
    if (LLSDSerialize::fromXML(root, in) == LLSDParser::PARSE_FAILURE ||
        !root["version"].isInteger() || root["version"].asInteger() != 1 ||
        !root["account"].isUUID() || root["account"].asUUID() != gAgentID ||
        !validSettings(root["settings"], [](const Feature& feature)
        {
            LLControlVariable* control = gSavedSettings.getControl(feature.setting);
            return control ? control->type() : TYPE_COUNT;
        }) || !root["actors"].isArray() ||
        !root["rig_mask"].isInteger() || root["rig_mask"].asInteger() < 0 ||
        root["rig_mask"].asInteger() >= (1 << ALCineLightRigManager::SLOT_COUNT) ||
        !root["rig_selected"].isInteger() || root["rig_selected"].asInteger() < 0 ||
        root["rig_selected"].asInteger() >= ALCineLightRigManager::SLOT_COUNT ||
        !root["rig_group"].isBoolean() ||
        !ALAvatarLightResponse::validDirectorSnapshot(root["light_response"]))
    {
        reason = "The last ALL OFF backup is invalid; nothing restored";
        return false;
    }
    const S32 selected = root["rig_selected"].asInteger();
    const S32 mask = root["rig_mask"].asInteger();
    if (root["settings"]["CineLightRigEnabled"].asBoolean() != ((mask & (1 << selected)) != 0))
    {
        reason = "The last ALL OFF rig backup is inconsistent; nothing restored";
        return false;
    }
    std::set<LLUUID> ids;
    for (const LLSD& actor : llsd::inArray(root["actors"]))
    {
        if (!actor.isMap() || !actor["id"].isUUID() || !actor["fx"].isBoolean() ||
            !actor["gaze"].isBoolean() || !ids.insert(actor["id"].asUUID()).second)
        {
            reason = "The last ALL OFF actor backup is invalid; nothing restored";
            return false;
        }
    }
    for (const Feature& feature : FEATURES)
    {
        if (!gSavedSettings.getControl(feature.setting))
        {
            reason = std::string("Cannot restore missing setting: ") + feature.setting;
            return false;
        }
    }
    reason.clear();
    return true;
}

void apply(const LLSD& root, bool restoring)
{
    ALExposureSettingsHistory::initialize();
    ALExposureSettingsHistory::ScopedBatch history(restoring ? "Restore last reset" : "ALL OFF");
    applySettings(root["settings"], restoring, [&](const Feature& feature, const LLSD& saved)
    {
        if (feature.operation == Operation::Rig)
        {
            auto& rigs = ALCineLightRigManager::instance();
            if (restoring) rigs.restoreEnabledMask(root["rig_mask"].asInteger(),
                static_cast<ALCineLightRigManager::Slot>(root["rig_selected"].asInteger()), root["rig_group"].asBoolean());
            else rigs.allOff();
        }
        else
        {
            LLControlVariable* control = gSavedSettings.getControl(feature.setting);
            // setValue is the same LLControlVariable path used by setting-bound kit switches.
            control->setValue(restoring ? saved : feature.operation == Operation::Default
                ? control->getDefault() : LLSD(false));
            if (feature.operation == Operation::Switcher && !control->get().asBoolean())
            {
                // The page's disarm cleanup normally runs on the next tick.
                // Finish releasing its temporary camera-enable lease now,
                // before the subsequent CinematicCamEnabled=false write.
                // A disarmed tick only releases ownership; it cannot start a shot.
                ALDirectorSwitcher::instance().tick(0.0);
            }
        }
    });
}
}

bool atomicWrite(const std::string& path, const std::string& bytes)
{
    static U32 sequence = 0;
    const std::string temporary = path + ".tmp-" + std::to_string(LLApp::getPid()) + "-" + std::to_string(++sequence);
    try
    {
        llofstream out(temporary.c_str(), std::ios::binary);
        if (!out.is_open()) return false;
        out.write(bytes.data(), bytes.size());
        out.flush();
        out.close();
        llstat st;
        if (!out.fail() && LLFile::stat(temporary, &st) == 0 &&
            st.st_size == static_cast<decltype(st.st_size)>(bytes.size()) &&
            LLFile::rename(temporary, path) == 0) return true;
    }
    catch (...) {}
    LLFile::remove(temporary);
    return false;
}

bool available(std::string& reason)
{
    LLSD root;
    return readBackup(root, reason);
}

bool reset(std::string& message)
{
    if (!idle(message)) return false;
    // Resolve and validate the complete contract before even replacing the backup.
    for (const Feature& feature : FEATURES)
    {
        if (!gSavedSettings.getControl(feature.setting))
        {
            message = std::string("ALL OFF aborted: missing setting ") + feature.setting;
            return false;
        }
    }
    LLSD root;
    root["version"] = 1;
    root["account"] = gAgentID;
    root["settings"] = captureSettings([](const Feature& feature)
        { return gSavedSettings.getControl(feature.setting)->get(); });
    root["actors"] = actors();
    root["rig_mask"] = static_cast<S32>(ALCineLightRigManager::instance().enabledMask());
    root["rig_selected"] = static_cast<S32>(ALCineLightRigManager::instance().selectedSlot());
    root["rig_group"] = ALCineLightRigManager::instance().isRigGroup();
    if (!ALAvatarLightResponse::instance().directorSnapshot(root["light_response"], message)) return false;
    std::ostringstream xml;
    LLSDSerialize::toPrettyXML(root, xml);
    if (!atomicWrite(backupPath(), xml.str()))
    {
        message = "ALL OFF aborted: backup could not be saved; nothing changed";
        return false;
    }
    apply(root, false);
    S32 skipped = 0;
    applyActors(root["actors"], false, skipped);
    ALAvatarLightResponse::instance().clearForDirector();
    message = "ALL OFF: features off; Personal Lighting and Avatar Light Response reset";
    return true;
}

bool restore(std::string& message)
{
    if (!idle(message)) return false;
    LLSD root;
    if (!readBackup(root, message)) return false;
    // ALR disk replacement can fail: do it before changing any live state/settings.
    if (!ALAvatarLightResponse::instance().restoreDirectorSnapshot(root["light_response"], message)) return false;
    apply(root, true);
    S32 skipped = 0;
    applyActors(root["actors"], true, skipped);
    message = skipped ? llformat("Restored last reset; skipped %d missing actor(s)", skipped) : "Restored last reset";
    return true;
}
}
