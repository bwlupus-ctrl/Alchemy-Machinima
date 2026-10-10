#include "llviewerprecompiledheaders.h"

#include "alpanelrigrim.h"

#include "alkitpresets.h"
#include "llpanel.h"

namespace
{
const std::vector<std::string>& rimSettings()
{
    static const std::vector<std::string> settings={
        "CineRigRimMasterGain","CineRigRimRoughnessSoften","CineRigRimBackSoftness",
        "CineRigRimTint","CineRigRimShadow","CineRigRimIncludeAlpha","CineRigRimIncludeBounce",
        "CineRigRimSmoothness","CineRigRimAntiShimmer","CineRigRimSilhouette",
        "CineRigRimSilhouetteWidth","CineRigRimSilhouetteUnits","CineRigRimSilhouetteSoft",
        "CineRigRimActorsOnly","CineRigRimWrapEnabled","CineRigRimWrapStrength",
        "CineRigRimWrapWidth","CineRigRimWrapBlur","CineRigRimWrapSaturation","CineRigRimWrapClamp",
        "CineRigRimKeyGain","CineRigRimKeySharpness","CineRigRimKeyWrap","CineRigRimKeyBackBias",
        "CineRigRimFillGain","CineRigRimFillSharpness","CineRigRimFillWrap","CineRigRimFillBackBias",
        "CineRigRimRimGain","CineRigRimRimSharpness","CineRigRimRimWrap","CineRigRimRimBackBias",
        "CineRigRimBgGain","CineRigRimBgSharpness","CineRigRimBgWrap","CineRigRimBgBackBias",
        "CineRigRimTronMode","CineRigRimTronMix","CineRigRimTronColorSource"};
    return settings;
}

class RigRimPresetProvider final : public ALKitPresetProvider
{
public:
    RigRimPresetProvider()
        : mStore("rig_rim",rimSettings())
    {
    }
    const std::string& key()const override
    {
        return mKey;
    }
    void list(std::vector<Entry>& out)const override
    {
        for(const std::string& name:ALRigRimController::presetNames())
        {
            out.push_back({"b:"+name,name,true});
        }
        for(const std::string& name:mStore.names())
        {
            out.push_back({"u:"+name,name,false});
        }
    }
    bool apply(const std::string& id)override
    {
        if(id.compare(0,2,"b:")==0)
        {
            return ALRigRimController::applyPresetByName(id.substr(2));
        }
        return id.compare(0,2,"u:")==0&&mStore.load(id.substr(2));
    }
    bool canSaveUser()const override
    {
        return true;
    }
    bool saveUser(const std::string& name)override
    {
        return mStore.save(name);
    }
    bool canDelete(const std::string& id)const override
    {
        return id.compare(0,2,"u:")==0;
    }
    bool deleteUser(const std::string& id)override
    {
        return canDelete(id)&&mStore.remove(id.substr(2));
    }
    U64 revision()const override
    {
        return mStore.revision();
    }
    const std::vector<std::string>& trackedSettings()const override
    {
        return rimSettings();
    }
private:
    std::string mKey="rig_rim";
    mutable ALKitSnapshotStore mStore;
};

void registerProvider()
{
    static bool registered=false;
    if(registered)
    {
        return;
    }
    registered=true;
    ALKitPresetRegistry::instance().add(std::make_unique<RigRimPresetProvider>());
}
}

static LLPanelInjector<ALPanelRigRim> register_panel_rig_rim("panel_rig_rim");

ALPanelRigRim::ALPanelRigRim()
{
    registerProvider();
}

bool ALPanelRigRim::postBuild()
{
    registerProvider();
    mRim.attach(this);
    return LLPanel::postBuild();
}

void ALPanelRigRim::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALPanelRigRim draw");
    if(!isWidgetOnScreen(this))
    {
        return;
    }
    mRim.tickFrame();
    if(mRefreshClock.due())
    {
        mRim.tickRefresh();
    }
    LLPanel::draw();
}

void ALPanelRigRim::onVisibilityChange(bool visible)
{
    if(visible)
    {
        mRim.onReveal();
        mRefreshClock.forceDue();
    }
    LLPanel::onVisibilityChange(visible);
}
