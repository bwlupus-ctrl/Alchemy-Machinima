#include "llviewerprecompiledheaders.h"
#include "aldiopterpresetbank.h"
#include "alkitpresets.h"
#include "alpresetlook.h"
#include "alrotoink.h"
#include "altron.h"
#include "alsteppedanim.h"
#include "alprojectorshaftpresets.h"
#include "alpanelweathersettings.h"
#include "alkitpage.h"
#include "alkitbinding.h"
#include "alkithistory.h"
#include "alsettingsundohistory.h"
#include "alkitshell.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llscrolllistitem.h"
#include "llscrolllistcell.h"
#include "lldir.h"
#include "llfile.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <map>

namespace ALPresetLook
{
namespace
{
S32 sLoadDepth = 0;
ALKitHistory* sLoadHistory = nullptr;

std::vector<std::string> select(std::initializer_list<const char*> prefixes,
                                std::initializer_list<const char*> excluded = {})
{
    struct Collector final : LLControlGroup::ApplyFunctor
    {
        std::vector<std::string> result;
        std::initializer_list<const char*> prefixes, excluded;
        void apply(const std::string& name, LLControlVariable*) override
        {
            auto matches = [&name](const char* prefix) { return name.compare(0, strlen(prefix), prefix) == 0; };
            if (std::any_of(prefixes.begin(), prefixes.end(), matches) &&
                !std::any_of(excluded.begin(), excluded.end(), matches)) result.push_back(name);
        }
    } collector;
    collector.prefixes = prefixes;
    collector.excluded = excluded;
    gSavedSettings.applyToAll(&collector);
    std::sort(collector.result.begin(), collector.result.end());
    return collector.result;
}
}

const std::vector<std::string>& owned(Family family)
{
    // A look preset owns LOOK settings only. Never own targeting/who (actor-only,
    // subject), shot placement/transform (including every settings.xml "Shot key"),
    // user calibration/preferences (stick feel, camera smoothing/zoom time),
    // performance/quality knobs, compile-time shader settings (handleSetShaderChanged),
    // or master enables. Prefix sets require explicit exclusions as well as the
    // Shot key metadata filter below; optional authored blocks remain sparse.
    static std::map<Family, std::vector<std::string>> sets;
    auto found = sets.find(family);
    if (found != sets.end()) return found->second;
    std::vector<std::string> keys;
    switch (family)
    {
    case LIGHT_SHAPE:
        keys = {"RenderLocalLightDefaultRadius", "RenderLocalLightHighlightScale"};
        break;
    case RIM:
        keys = select({"CineRigRim"}, {"CineRigRimEnabled", "CineRigRimDebug", "CineRigRimIncludeProbes", "CineRigRimTron", "CineRigRimActorsOnly"});
        break;
    case RIM_CUSTOM:
        keys = owned(RIM);
        for (const char* key : {"CineRigRimTronMode", "CineRigRimTronMix", "CineRigRimTronColorSource"}) keys.emplace_back(key);
        break;
    case ROTO:
        keys = select({"CineOutline"}, {"CineOutlineEnabled", "CineOutlineLayer", "CineOutlineSubject", "CineOutlineAnim"});
        break;
    case TRON:
        keys = select({"Tron", "CineRigRimTron"},
            {"TronEnabled", "TronLayer", "TronSubject", "TronAnim", "TronPresetAlso", "TronGridOriginGlobal",
             "TronGridSubject", "TronNoPostScale", "TronBloomMeterScale", "TronTrailTarget", "TronTrailMaxActors",
             "TronTrailMaxPoints", "TronTrailMaxSegments", "TronTrailClearOnCut"});
        break;
    case INFRARED:
        keys = select({"RenderInfrared"}, {"RenderInfraredEnabled", "RenderInfraredDebugView", "RenderInfraredPresetSetsGrain"});
        break;
    case SOFT_EDGE:
        keys = select({"RenderSoftEdge"}, {"RenderSoftEdgeEnabled", "RenderSoftEdgeAnchor", "RenderSoftEdgeCenter", "RenderSoftEdgeShowMask"});
        break;
    case HAZE:
        keys = select({"CineHaze"}, {"CineHazeEnable", "CineHazeDebug"});
        break;
    case FLARE:
        keys = select({"RenderLensFlare", "RenderCineLensFlare"},
            {"RenderLensFlareMaster", "RenderLensFlareStrength", "RenderCineLensFlareStrength",
             "RenderCineLensFlareIntensity", "RenderCineLensFlarePreset", "RenderLensFlareOcclusionTaps"});
        break;
    case NIGHT_MASK:
        keys = select({"CineLightRigNightMask"}, {"CineLightRigNightMaskEnabled", "CineLightRigNightMaskTarget", "CineLightRigNightMaskGizmo"});
        break;
    case GRAD_ND:
    case POLARIZER:
    case LENS_FILTER:
        // Named modes resolve in the renderer; Custom sliders and filter amount
        // belong to the user, not to a named mode selection.
        break;
    case SHAKE:
        keys = select({"FlycamOperator"}, {"FlycamOperatorEnabled", "FlycamOperatorSimulationHz", "FlycamOperatorSeed"});
        break;
    case WEIGHTED:
        keys = {"FlycamWeightedWeight", "FlycamWeightedRotationShare", "FlycamWeightedTauMax", "FlycamWeightedCurve"};
        break;
    case TRON_PALETTE:
        keys = {"TronColorPrimary", "TronColorSecondary", "TronColorAccent", "TronColorPulse"};
        break;
    case DRONE:
        keys = select({"FlycamDrone"}, {"FlycamDronePreset", "FlycamDroneSimulationHz", "FlycamDroneMoveDeadzone", "FlycamDroneLookDeadzone",
            "FlycamDroneInvertLook", "FlycamDroneResumeMomentum", "FlycamDroneSaturation", "FlycamDroneExpo"});
        break;
    case LIGHT_RIG:
        keys = select({"CineLightRigKey", "CineLightRigFill", "CineLightRigRim", "CineLightRigBg"},
            {"CineLightRigKeyActorOnly", "CineLightRigFillActorOnly", "CineLightRigRimActorOnly", "CineLightRigBgActorOnly"});
        for (const char* key : {"CineLightRigRadius", "CineLightRigRatioLock", "CineLightRigRatio", "CineLightRigMasterEV",
             "CineLightRigMasterTempMired", "CineLightRigHeadroomStops", "CineLightRigBounceEnabled",
             "CineLightRigBounceRatio", "CineLightRigMirror",
             "CineLightRigShadowMode", "CineLightRigCatchlight", "CineLightRigCatchlightEV", "CineLightRigCatchlightSize",
             "CineLightRigCatchlightAngle", "CineLightRigCookieUUID"})
            keys.emplace_back(key);
        // Volumetric globals (including quality, master and shader permutations)
        // are applied only when explicitly authored by the optional block.
        break;
    }
    keys.erase(std::remove_if(keys.begin(), keys.end(), [](const std::string& name)
        {
            LLControlVariable* control = gSavedSettings.getControl(name);
            return control && control->getComment().find("Shot key") != std::string::npos;
        }), keys.end());
    return sets.emplace(family, std::move(keys)).first->second;
}

std::vector<std::string> pageSettings(const std::string& page)
{
    std::vector<std::string> keys;
    auto add = [&keys](const std::vector<std::string>& more) { keys.insert(keys.end(), more.begin(), more.end()); };
    if (page == "light_group/light_rim_tab") add(owned(RIM_CUSTOM));
    if (page == "look_group/stylize_tab") add(owned(ROTO));
    if (page == "look_group/tron_tab") { add(owned(TRON)); add(owned(ROTO)); add(owned(RIM)); }
    if (page == "lens_group/filters_tab")
    {
        add(owned(INFRARED)); add(owned(SOFT_EDGE)); add(owned(GRAD_ND)); add(owned(POLARIZER)); add(owned(LENS_FILTER));
        add(infraredGrainSettings()); // opt-in IR grain hint
    }
    if (page == "camera_group/operator_tab") add(owned(SHAKE));
    if (page == "camera_group/takes_tab") { add(owned(WEIGHTED)); add(owned(DRONE)); }
    if (page == "light_group/cine_light_rig_tab") { add(owned(LIGHT_RIG)); add(owned(FLARE)); add(owned(NIGHT_MASK)); }
    if (page == "world_group/atmosphere_tab") add(owned(HAZE));
    if (page == "light_group/projector_shafts_tab") add(ALProjectorShaftPresets::pageSettings());
    if (page == "world_group/weather_tab") add(ALPanelWeatherSettings::trackedSettings());
    if (page == "look_group/stepped_tab") { add(ALSteppedAnim::presetSettings()); keys.emplace_back("StepAnimEnabled"); }
    return keys;
}
const std::vector<std::string>& infraredGrainSettings()
{
    static const std::vector<std::string> keys = {"RenderFilmGrainAmount"};
    return keys;
}
std::vector<std::string> fixtureSettings(const std::string& prefix)
{
    std::vector<std::string> keys;
    for (const char* suffix : {"FixtureMode", "FixturePreset", "Kelvin", "SourceSizeM", "Beam", "GelSlot0", "GelSlot1", "GelSlot2"})
        keys.push_back(prefix + suffix);
    return keys;
}

void reset(const std::vector<std::string>& settings)
{
    for (const std::string& name : settings)
        if (LLControlVariable* control = gSavedSettings.getControl(name)) control->resetToDefault(true);
}
LLSD snapshot(const std::vector<std::string>& settings)
{
    LLSD values = LLSD::emptyMap();
    for (const std::string& name : settings)
        if (LLControlVariable* control = gSavedSettings.getControl(name)) values[name] = control->get();
    return values;
}
void apply(const std::vector<std::string>& settings, const LLSD& values)
{
    ALScopedPresetMaterializing preset_guard;
    for (const std::string& name : settings)
        if (values.has(name))
            if (LLControlVariable* control = gSavedSettings.getControl(name)) control->set(values[name]);
}
Load::Load(const std::string& label)
{
    // Focus is not provenance: timers/cues must not join or close an open user
    // gesture, even while its page has keyboard focus or mouse capture.
    if (sLoadDepth++ == 0 && !ALKitHistory::hasInputSource())
        mSuspend = std::make_unique<ALKitHistory::ScopedSuspend>();
    if (!ALKitHistory::suspended() && !replaying() && ALKitHistory::hasInputSource())
        mBatch = std::make_unique<ALExposureSettingsHistory::ScopedBatch>(label);
    if (sLoadDepth == 1 && !ALKitHistory::suspended() && !replaying() && ALKitHistory::hasInputSource())
        if (ALKitPage* page = dynamic_cast<ALKitPage*>(ALKitHistory::inputPage()))
            if (ALKitShell* shell = ALKitShell::find(page))
            {
                mHistory = &shell->history();
                sLoadHistory = mHistory;
                mHistory->ensureGesture(page->pageKey(), label, true);
            }
}
void Load::recordState(const std::string& key, std::function<bool(bool, std::string&)> replay,
                       std::function<bool(bool, std::string&)> validate)
{
    if (sLoadHistory && ALKitHistory::hasInputSource() && !ALKitHistory::suspended() && !replaying())
        sLoadHistory->recordCustom(ALKitHistory::inputPage(), key, "Apply preset", std::move(replay), true, std::move(validate));
}
bool replaying() { return ALKitHistory::replaying(); }
Load::~Load()
{
    if (mHistory)
    {
        mHistory->finishGesture();
        sLoadHistory = nullptr;
    }
    --sLoadDepth;
}
}

static LLDefaultChildRegistry::Register<ALKitPresetStrip> register_preset_strip("kit_preset_strip");

namespace
{
std::string presetPrefix(const std::string& page)
{
    std::string  prefix = page;
    const size_t slash  = prefix.rfind('/');
    if (slash != std::string::npos)
        prefix = prefix.substr(slash + 1);
    if (prefix.size() > 4 && prefix.compare(prefix.size() - 4, 4, "_tab") == 0)
        prefix.erase(prefix.size() - 4);
    return prefix;
}
} // namespace

void ALKitPresetRegistry::add(std::unique_ptr<ALKitPresetProvider> p)
{
    if (!p)
        return;
    for (auto& existing : mProviders)
    {
        if (existing->key() == p->key())
        {
            existing = std::move(p);
            return;
        }
    }
    mProviders.push_back(std::move(p));
}
ALKitPresetProvider* ALKitPresetRegistry::get(const std::string& k)
{
    for (auto& p : mProviders)
        if (p->key() == k)
            return p.get();
    return nullptr;
}
ALKitSnapshotStore::ALKitSnapshotStore(std::string k, std::vector<std::string> s) : mKey(std::move(k)), mSettings(std::move(s))
{
}

std::vector<std::string> ALKitSnapshotStore::names() const
{
    std::vector<std::string> result;
    if (!ensureLoaded())
        return result;
    for (LLSD::map_const_iterator it = mCache.beginMap(); it != mCache.endMap(); ++it)
    {
        result.push_back(it->first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool ALKitSnapshotStore::save(const std::string& name)
{
    if (name.empty())
        return false;
    if (!ensureLoaded())
    {
        LLNotificationsUtil::add("SystemMessageTip",
                                 LLSD().with("MESSAGE", "Preset file is unreadable; saved presets were left untouched."));
        return false;
    }
    LLSD root   = mCache;
    LLSD values = LLSD::emptyMap();
    for (const std::string& setting : mSettings)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(setting).get())
        {
            values[setting] = control->get();
        }
    }
    root[name] = values;
    if (!write(root))
        return false;
    mCache = root;
    ++mRevision;
    return true;
}

bool ALKitSnapshotStore::load(const std::string& name)
{
    if (ALPresetLook::replaying()) return false;
    if (!ensureLoaded() || !mCache.has(name) || !mCache[name].isMap())
        return false;
    const LLSD& values = mCache[name];
    ALPresetLook::Load load;
    ALPresetLook::reset(mSettings);
    for (const std::string& setting : mSettings)
    {
        if (values.has(setting))
            if (LLControlVariable* control = gSavedSettings.getControl(setting).get())
            {
                control->set(values[setting]);
            }
    }
    return true;
}

bool ALKitSnapshotStore::values(const std::string& name, LLSD& out) const
{
    if (!ensureLoaded() || !mCache.has(name) || !mCache[name].isMap())
        return false;
    out = mCache[name];
    return true;
}

bool ALKitSnapshotStore::remove(const std::string& name)
{
    if (!ensureLoaded())
    {
        LLNotificationsUtil::add("SystemMessageTip",
                                 LLSD().with("MESSAGE", "Preset file is unreadable; saved presets were left untouched."));
        return false;
    }
    if (!mCache.has(name))
        return false;
    LLSD root = mCache;
    root.erase(name);
    if (!write(root))
        return false;
    mCache = root;
    ++mRevision;
    return true;
}

bool ALKitSnapshotStore::read(LLSD& root) const
{
    root = LLSD::emptyMap();
    llifstream input(path().c_str());
    if (!input)
        return true;
    if (LLSDSerialize::fromXML(root, input) == LLSDParser::PARSE_FAILURE || !root.isMap())
    {
        LL_WARNS("DirectorKit") << "Invalid preset store " << path() << LL_ENDL;
        root = LLSD::emptyMap();
        return false;
    }
    return true;
}

bool ALKitSnapshotStore::ensureLoaded() const
{
    if (mLoaded)
        return mLoadValid;
    mLoadValid = read(mCache);
    mLoaded    = true;
    return mLoadValid;
}

bool ALKitSnapshotStore::write(const LLSD& root) const
{
    const std::string directory = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "director_presets");
    if (!LLFile::isdir(directory))
    {
        LLFile::mkdir(directory);
    }
    const std::string destination = path();
    const std::string temporary   = destination + ".tmp";
    const std::string backup      = destination + ".bak";
    llofstream        output(temporary.c_str());
    if (!output.is_open())
        return false;
    const S32 serialized = LLSDSerialize::toPrettyXML(root, output);
    output.flush();
    const bool succeeded = serialized > 0 && output.good();
    output.close();
    if (!succeeded || output.fail())
    {
        LLFile::remove(temporary);
        return false;
    }
    bool moved_existing = false;
    if (LLFile::exists(destination))
    {
        LLFile::remove(backup, ENOENT);
        if (LLFile::rename(destination, backup) != 0)
        {
            LLFile::remove(temporary);
            return false;
        }
        moved_existing = true;
    }
    if (LLFile::rename(temporary, destination) != 0)
    {
        if (moved_existing)
            LLFile::rename(backup, destination);
        LLFile::remove(temporary, ENOENT);
        return false;
    }
    return true;
}

std::string ALKitSnapshotStore::path() const
{ return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "director_presets", mKey + ".xml"); }

ALKitPresetStrip::Params::Params() : provider("provider"), page("page")
{
}

ALKitPresetStrip::ALKitPresetStrip(const Params& params) : LLPanel(params), mProviderKey(params.provider), mPage(params.page)
{
}

bool ALKitPresetStrip::postBuild()
{
    if (!LLPanel::postBuild())
    {
        return false;
    }
    mProvider = ALKitPresetRegistry::instance().get(mProviderKey);
    if (!mProvider)
    {
        LL_WARNS("DirectorKit") << "Missing preset provider " << mProviderKey << LL_ENDL;
        return true;
    }
    const std::string  prefix = presetPrefix(mPage);
    LLComboBox::Params combo_params;
    // Preserve the shared shaft panel's lookup contract on the v2 page.
    combo_params.name = mProviderKey == "projector_shafts" ? "ps_preset" : prefix + "_preset";
    combo_params.rect = LLRect(0, 24, 190, 0);
    mCombo            = LLUICtrlFactory::create<LLComboBox>(combo_params);
    addChild(mCombo);
    mCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSelect(); });

    LLLineEditor::Params name_params;
    name_params.name                 = prefix + "_preset_name";
    name_params.rect                 = LLRect(0, 24, 190, 0);
    name_params.visible              = false;
    name_params.commit_on_focus_lost = false;
    mName                            = LLUICtrlFactory::create<LLLineEditor>(name_params);
    addChild(mName);
    mName->setCommitCallback([this](LLUICtrl*, const LLSD&) { save(); });

    LLButton::Params button_params;
    button_params.name  = prefix + "_preset_save";
    button_params.label = "Save as...";
    button_params.rect  = LLRect(196, 24, 270, 0);
    mSave               = LLUICtrlFactory::create<LLButton>(button_params);
    addChild(mSave);
    mSave->setCommitCallback([this](LLUICtrl*, const LLSD&) { showSaveEditor(); });
    button_params.name  = prefix + "_preset_delete";
    button_params.label = "Delete";
    button_params.rect  = LLRect(276, 24, 334, 0);
    mDelete             = LLUICtrlFactory::create<LLButton>(button_params);
    addChild(mDelete);
    mDelete->setCommitCallback([this](LLUICtrl*, const LLSD&) { remove(); });

    for (const std::string& setting : mProvider->trackedSettings())
    {
        mConnections.emplace_back(
            ALKitSettingWatch::instance().watch(setting, [this](const LLSD&, const LLSD&) { trackedSettingChanged(); }));
    }
    mShaftEngine = gSavedSettings.getU32("BDMergeProjectorVolumetricsEngine");
    refresh();
    return true;
}

void ALKitPresetStrip::refresh()
{
    if (!mProvider || !mCombo)
    {
        return;
    }
    const std::string selected = mCombo->getSelectedValue().asString();
    mCombo->clearRows();
    mCombo->add("Custom", LLSD(""));
    std::vector<ALKitPresetProvider::Entry> entries;
    mProvider->list(entries);
    bool had_builtin = false;
    bool had_user    = false;
    for (const ALKitPresetProvider::Entry& entry : entries)
    {
        if (entry.builtin && !had_builtin)
        {
            mCombo->add("Built-in", LLSD("header:builtin"), ADD_BOTTOM, false);
            had_builtin = true;
        }
        if (!entry.builtin && !had_user)
        {
            mCombo->add("-- Saved --", LLSD("header:saved"), ADD_BOTTOM, false);
            had_user = true;
        }
        LLScrollListItem* item = mCombo->add(entry.label, LLSD(entry.id));
        if (item && item->getColumn(0))
            item->getColumn(0)->setToolTip(entry.tooltip);
    }
    if (!mAppliedId.empty() && mModified)
    {
        mCombo->add("Revert to " + mAppliedLabel, LLSD("revert:" + mAppliedId));
    }
    if (selected.empty() || !mCombo->setSelectedByValue(LLSD(selected), true))
    {
        mCombo->selectFirstItem();
    }
    if (mModified && !mAppliedLabel.empty())
    {
        mCombo->setLabel(LLStringExplicit(mAppliedLabel + " (modified)"));
    }
    mSave->setEnabled(mProvider->canSaveUser());
    mDelete->setEnabled(mProvider->canDelete(mCombo->getSelectedValue().asString()));
}

void ALKitPresetStrip::onSelect()
{
    ALKitHistory::ScopedSource source(this);
    if (!mProvider || !mCombo)
    {
        return;
    }
    std::string id = mCombo->getSelectedValue().asString();
    if (id.empty())
    {
        mDelete->setEnabled(false);
        return;
    }
    if (id.compare(0, 7, "revert:") == 0)
    {
        id.erase(0, 7);
    }
    ALExposureSettingsHistory::ScopedBatch batch("Apply preset");
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        shell->history().beginGesture(ALKitHistory::MOUSE, mPage, "Apply preset");
    }
    mApplying          = true;
    const bool applied = mProvider->apply(id);
    mApplying          = false;
    if (applied)
    {
        std::string label = mCombo->getSelectedItemLabel();
        if (label.compare(0, 10, "Revert to ") == 0)
        {
            label.erase(0, 10);
        }
        captureApplied(id, label);
    }
    refresh();
}

void ALKitPresetStrip::showSaveEditor()
{
    if (!mProvider || !mProvider->canSaveUser())
    {
        return;
    }
    mCombo->setVisible(false);
    mName->setVisible(true);
    mName->setText(LLStringUtil::null);
    mName->setFocus(true);
}

void ALKitPresetStrip::save()
{
    if (!mProvider || !mName)
    {
        return;
    }
    std::string name = mName->getText();
    LLStringUtil::trim(name);
    if (!name.empty() && mProvider->saveUser(name))
    {
        captureApplied("u:" + name, name);
    }
    mName->setVisible(false);
    mCombo->setVisible(true);
    refresh();
    mCombo->setSelectedByValue(LLSD(mAppliedId), true);
}

void ALKitPresetStrip::remove()
{
    if (!mProvider || !mCombo)
    {
        return;
    }
    const std::string id = mCombo->getSelectedValue().asString();
    if (!mProvider->canDelete(id))
    {
        return;
    }
    const std::string          label  = mCombo->getSelectedItemLabel();
    LLHandle<ALKitPresetStrip> handle = getDerivedHandle<ALKitPresetStrip>();
    LLNotificationsUtil::add("GenericAlertYesCancel", LLSD().with("MESSAGE", "Delete saved preset '" + label + "'?"), LLSD(),
                             [handle, id](const LLSD& notification, const LLSD& response)
                             {
                                 if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
                                 {
                                     return;
                                 }
                                 if (ALKitPresetStrip* strip = handle.get())
                                 {
                                     if (strip->mProvider && strip->mProvider->deleteUser(id))
                                     {
                                         if (strip->mAppliedId == id)
                                         {
                                             strip->mAppliedId.clear();
                                             strip->mAppliedLabel.clear();
                                             strip->mAppliedValues.clear();
                                         }
                                         strip->refresh();
                                     }
                                 }
                             });
}

void ALKitPresetStrip::captureApplied(const std::string& id, const std::string& label)
{
    mAppliedId    = id;
    mAppliedLabel = label;
    mAppliedValues.clear();
    for (const std::string& setting : mProvider->trackedSettings())
    {
        LLControlVariable* control = gSavedSettings.getControl(setting).get();
        mAppliedValues.push_back(control ? control->get() : LLSD());
    }
    mModified = false;
}

void ALKitPresetStrip::trackedSettingChanged()
{
    if (mProviderKey == "projector_shafts")
    {
        const U32 engine = gSavedSettings.getU32("BDMergeProjectorVolumetricsEngine");
        if (engine != mShaftEngine)
        {
            mShaftEngine = engine;
            mAppliedId.clear(); mAppliedLabel.clear(); mAppliedValues.clear(); mModified = false;
            refresh();
            return;
        }
    }
    if (mApplying || mAppliedId.empty())
    {
        return;
    }
    const bool modified = isModified();
    if (modified != mModified)
    {
        mModified = modified;
        refresh();
    }
}

bool ALKitPresetStrip::isModified() const
{
    const std::vector<std::string>& settings = mProvider->trackedSettings();
    if (settings.size() != mAppliedValues.size())
    {
        return true;
    }
    for (size_t index = 0; index < settings.size(); ++index)
    {
        LLControlVariable* control = gSavedSettings.getControl(settings[index]).get();
        if (!control || control->get() != mAppliedValues[index])
        {
            return true;
        }
    }
    return false;
}
