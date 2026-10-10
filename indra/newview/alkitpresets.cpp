#include "llviewerprecompiledheaders.h"
#include "alkitpresets.h"
#include "alkitbinding.h"
#include "alkithistory.h"
#include "alkitshell.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llfile.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"

#include <algorithm>

static LLDefaultChildRegistry::Register<ALKitPresetStrip> register_preset_strip("kit_preset_strip");

void ALKitPresetRegistry::add(std::unique_ptr<ALKitPresetProvider> p)
{
    if(!p)return;
    for(auto& existing:mProviders)
    {
        if(existing->key()==p->key())
        {
            existing=std::move(p);
            return;
        }
    }
    mProviders.push_back(std::move(p));
}
ALKitPresetProvider* ALKitPresetRegistry::get(const std::string& k)
{
    for(auto& p:mProviders)if(p->key()==k)return p.get();
    return nullptr;
}
ALKitSnapshotStore::ALKitSnapshotStore(std::string k,std::vector<std::string>s):mKey(std::move(k)),mSettings(std::move(s))
{
}

std::vector<std::string> ALKitSnapshotStore::names()const
{
    LLSD root;
    std::vector<std::string> result;
    if(!read(root))return result;
    for(LLSD::map_const_iterator it=root.beginMap();it!=root.endMap();++it)
    {
        result.push_back(it->first);
    }
    std::sort(result.begin(),result.end());
    return result;
}

bool ALKitSnapshotStore::save(const std::string& name)
{
    if(name.empty())return false;
    LLSD root;
    read(root);
    LLSD values=LLSD::emptyMap();
    for(const std::string& setting:mSettings)
    {
        if(LLControlVariable* control=gSavedSettings.getControl(setting).get())
        {
            values[setting]=control->get();
        }
    }
    root[name]=values;
    if(!write(root))return false;
    ++mRevision;
    return true;
}

bool ALKitSnapshotStore::load(const std::string& name)
{
    LLSD root;
    if(!read(root)||!root.has(name)||!root[name].isMap())return false;
    const LLSD& values=root[name];
    for(LLSD::map_const_iterator it=values.beginMap();it!=values.endMap();++it)
    {
        if(LLControlVariable* control=gSavedSettings.getControl(it->first).get())
        {
            control->set(it->second);
        }
    }
    return true;
}

bool ALKitSnapshotStore::remove(const std::string& name)
{
    LLSD root;
    if(!read(root)||!root.has(name))return false;
    root.erase(name);
    if(!write(root))return false;
    ++mRevision;
    return true;
}

bool ALKitSnapshotStore::read(LLSD& root)const
{
    root=LLSD::emptyMap();
    llifstream input(path().c_str());
    if(!input)return true;
    if(LLSDSerialize::fromXML(root,input)==LLSDParser::PARSE_FAILURE||!root.isMap())
    {
        LL_WARNS("DirectorKit")<<"Invalid preset store "<<path()<<LL_ENDL;
        root=LLSD::emptyMap();
        return false;
    }
    return true;
}

bool ALKitSnapshotStore::write(const LLSD& root)const
{
    const std::string directory=gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,"director_presets");
    if(!LLFile::isdir(directory))
    {
        LLFile::mkdir(directory);
    }
    llofstream output(path().c_str());
    return output&&LLSDSerialize::toPrettyXML(root,output)>0;
}

std::string ALKitSnapshotStore::path()const
{
    return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,"director_presets",mKey+".xml");
}

ALKitPresetStrip::Params::Params()
    : provider("provider"),
      page("page")
{
}

ALKitPresetStrip::ALKitPresetStrip(const Params& params)
    : LLPanel(params),
      mProviderKey(params.provider),
      mPage(params.page)
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
    LLComboBox::Params combo_params;
    combo_params.name = "kit_preset_combo";
    combo_params.rect = LLRect(0, 24, 190, 0);
    mCombo = LLUICtrlFactory::create<LLComboBox>(combo_params);
    addChild(mCombo);
    mCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSelect(); });

    LLLineEditor::Params name_params;
    name_params.name = "kit_preset_name";
    name_params.rect = LLRect(0, 24, 190, 0);
    name_params.visible = false;
    name_params.commit_on_focus_lost = false;
    mName = LLUICtrlFactory::create<LLLineEditor>(name_params);
    addChild(mName);
    mName->setCommitCallback([this](LLUICtrl*, const LLSD&) { save(); });

    LLButton::Params button_params;
    button_params.name = "kit_preset_save";
    button_params.label = "Save as...";
    button_params.rect = LLRect(196, 24, 270, 0);
    mSave = LLUICtrlFactory::create<LLButton>(button_params);
    addChild(mSave);
    mSave->setCommitCallback([this](LLUICtrl*, const LLSD&) { showSaveEditor(); });
    button_params.name = "kit_preset_delete";
    button_params.label = "Delete";
    button_params.rect = LLRect(276, 24, 334, 0);
    mDelete = LLUICtrlFactory::create<LLButton>(button_params);
    addChild(mDelete);
    mDelete->setCommitCallback([this](LLUICtrl*, const LLSD&) { remove(); });

    for (const std::string& setting : mProvider->trackedSettings())
    {
        mConnections.emplace_back(ALKitSettingWatch::instance().watch(setting,
            [this](const LLSD&, const LLSD&) { trackedSettingChanged(); }));
    }
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
    bool had_user = false;
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
        mCombo->add(entry.label, LLSD(entry.id));
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
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        shell->history().beginGesture(ALKitHistory::MOUSE, mPage, "Apply preset");
    }
    mApplying = true;
    const bool applied = mProvider->apply(id);
    mApplying = false;
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
    const std::string label = mCombo->getSelectedItemLabel();
    LLHandle<ALKitPresetStrip> handle = getDerivedHandle<ALKitPresetStrip>();
    LLNotificationsUtil::add("GenericAlertYesCancel",
        LLSD().with("MESSAGE", "Delete saved preset '" + label + "'?"), LLSD(),
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
    mAppliedId = id;
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
