#include "llviewerprecompiledheaders.h"
#include "alkitbinding.h"
#include "alkitqol.h"
#include "alkitpage.h"
#include "llcontrol.h"
#include "lluictrl.h"
#include "llviewercontrol.h"
#include "lltimer.h"
#include "v3color.h"
#include "v4color.h"
#include <cstdlib>

namespace
{
// U32 controls are stored as the S32 bit pattern of the value (llcontrol.cpp), so a value
// above 2^31-1 is a negative LLSD integer.  Writing must therefore build the U32 first (from a
// real or string that may exceed S32 range) and wrap it once; reading unwraps it for the UI.
U32 llsdToU32(const LLSD& value)
{
    if (value.isReal())
    {
        const F64 real = value.asReal();
        if (!(real > 0.0))
            return 0;
        if (real >= 4294967295.0)
            return 4294967295u;
        return static_cast<U32>(real + 0.5);
    }
    if (value.isString())
    {
        const std::string text = value.asString();
        char*             end  = nullptr;
        const F64         real = strtod(text.c_str(), &end);
        if (end == text.c_str() || !(real > 0.0))
            return 0;
        return real >= 4294967295.0 ? 4294967295u : static_cast<U32>(real + 0.5);
    }
    return static_cast<U32>(value.asInteger()); // an integer already carries the bit pattern
}

LLSD unsignedControlValue(LLControlVariable* control, const LLSD& raw)
{
    if (control && control->type() == TYPE_U32 && raw.isInteger() && raw.asInteger() < 0)
        return LLSD(static_cast<LLSD::Real>(static_cast<U32>(raw.asInteger())));
    return raw;
}
} // namespace

bool alKitSetTypedControl(LLControlGroup& group, const std::string& name, const LLSD& value)
{
    LLControlVariable* control = group.getControl(name).get();
    if (!control || !value.isDefined())
        return false;

    switch (control->type())
    {
        case TYPE_S32:
            control->set(LLSD(value.asInteger()));
            break;
        case TYPE_U32:
            control->set(LLSD(static_cast<LLSD::Integer>(llsdToU32(value))));
            break;
        case TYPE_F32:
            control->set(LLSD(value.asReal()));
            break;
        case TYPE_BOOLEAN:
            if (value.isString())
            {
                bool converted = false;
                LLStringUtil::convertToBOOL(value.asString(), converted);
                control->set(LLSD(converted));
            }
            else
            {
                control->set(LLSD(value.asBoolean()));
            }
            break;
        case TYPE_STRING:
            control->set(LLSD(value.asString()));
            break;
        case TYPE_LLSD:
            control->set(value);
            break;
        case TYPE_COL3:
            control->set(LLColor3(value).getValue());
            break;
        case TYPE_COL4:
            control->set(LLColor4(value).getValue());
            break;
        default:
            control->set(value);
            break;
    }
    return true;
}

bool ALKitBinding::isDefault(S32 decimals) const
{ return ALKitQol::valuesEqual(get(), getDefault(), decimals); }

ALKitSettingBinding::ALKitSettingBinding(std::string name) : mName(std::move(name))
{
}

LLControlVariable* ALKitSettingBinding::control() const
{ return gSavedSettings.getControl(mName).get(); }

LLSD ALKitSettingBinding::get() const
{ return control() ? unsignedControlValue(control(), control()->get()) : LLSD(); }

LLSD ALKitSettingBinding::getDefault() const
{ return control() ? unsignedControlValue(control(), control()->getDefault()) : LLSD(); }

void ALKitSettingBinding::set(const LLSD& v)
{
    alKitSetTypedControl(gSavedSettings, mName, v);
}

void ALKitSettingBinding::reset()
{
    if (LLControlVariable* setting = control())
        setting->resetToDefault(true);
}

boost::signals2::connection ALKitSettingBinding::connectChanged(std::function<void()> cb)
{
    return ALKitSettingWatch::instance().watch(mName, [cb](const LLSD&, const LLSD&) { cb(); });
}

ALKitWidgetBinding::ALKitWidgetBinding(LLUICtrl* c, LLSD d) : mCtrl(c), mDefault(std::move(d))
{
}

LLSD ALKitWidgetBinding::get() const
{ return mCtrl ? mCtrl->getValue() : LLSD(); }

void ALKitWidgetBinding::set(const LLSD& v)
{
    if (mCtrl)
    {
        mCtrl->setValue(v);
        mCtrl->onCommit();
    }
}

boost::signals2::connection ALKitWidgetBinding::connectChanged(std::function<void()> cb)
{
    return mCtrl ? mCtrl->setCommitCallback([cb](LLUICtrl*, const LLSD&) { cb(); }) : boost::signals2::connection();
}

LLSD ALKitSettingsListBinding::get() const
{
    LLSD out = LLSD::emptyMap();
    for (const auto& n : mNames)
        if (LLControlVariable* c = gSavedSettings.getControl(n).get())
            out[n] = c->get();
    return out;
}

LLSD ALKitSettingsListBinding::getDefault() const
{
    LLSD out = LLSD::emptyMap();
    for (const auto& n : mNames)
        if (LLControlVariable* c = gSavedSettings.getControl(n).get())
            out[n] = c->getDefault();
    return out;
}

void ALKitSettingsListBinding::reset()
{
    for (const auto& n : mNames)
        if (LLControlVariable* c = gSavedSettings.getControl(n).get())
            c->resetToDefault(true);
}

boost::signals2::connection ALKitSettingsListBinding::connectChanged(std::function<void()> cb)
{
    if (mSources.empty())
        for (const auto& n : mNames)
            mSources.emplace_back(ALKitSettingWatch::instance().watch(n, [this](const LLSD&, const LLSD&) { mChanged(); }));
    return mChanged.connect(std::move(cb));
}

ALKitVecBinding::ALKitVecBinding(std::string setting, LLUICtrl* x, LLUICtrl* y, LLUICtrl* z) :
    mSetting(std::move(setting)),
    mX(x),
    mY(y),
    mZ(z)
{
    for (LLUICtrl* c : { mX, mY, mZ })
        if (c)
            mCommits.emplace_back(c->setCommitCallback([this](LLUICtrl*, const LLSD&) { commit(); }));
    if (LLControlVariable* c = gSavedSettings.getControl(mSetting).get())
        syncControls(c->get());
}

void ALKitVecBinding::commit()
{
    if (mSyncing)
        return;

    if (LLControlVariable* control = gSavedSettings.getControl(mSetting).get())
    {
        LLSD value = control->get();

        const LLSD edited = get();

        for (S32 i = 0; i < edited.size(); ++i)
            value[i] = edited[i];

        // Preserve the declared saved-setting type. In particular, Color3
        // controls must receive an LLColor3-shaped LLSD array of three reals.
        alKitSetTypedControl(gSavedSettings, mSetting, value);
    }
}

LLSD ALKitVecBinding::get() const
{
    LLSD out = LLSD::emptyArray();
    if (mX)
        out.append(mX->getValue());
    if (mY)
        out.append(mY->getValue());
    if (mZ)
        out.append(mZ->getValue());
    return out;
}

LLSD ALKitVecBinding::getDefault() const
{
    if (LLControlVariable* c = gSavedSettings.getControl(mSetting).get())
        return c->getDefault();
    return LLSD();
}

bool ALKitVecBinding::isDefault(S32 decimals) const
{
    const LLSD value = get();
    const LLSD all_defaults = getDefault();
    LLSD defaults = LLSD::emptyArray();
    for (S32 i = 0; i < value.size(); ++i)
        defaults.append(all_defaults[i]);
    return ALKitQol::valuesEqual(value, defaults, decimals);
}

void ALKitVecBinding::set(const LLSD& v)
{
    syncControls(v);
    commit();
}

void ALKitVecBinding::syncControls(const LLSD& v)
{
    mSyncing = true;
    if (mX && v.size() > 0)
        mX->setValue(v[0]);
    if (mY && v.size() > 1)
        mY->setValue(v[1]);
    if (mZ && v.size() > 2)
        mZ->setValue(v[2]);
    mSyncing = false;
}

boost::signals2::connection ALKitVecBinding::connectChanged(std::function<void()> cb)
{
    return ALKitSettingWatch::instance().watch(mSetting,
                                               [this, cb](const LLSD& v, const LLSD&)
                                               {
                                                   syncControls(v);
                                                   cb();
                                               });
}

static std::string bindingKey(LLView* root, const std::string& widget)
{ return llformat("%p:%s", root, widget.c_str()); }

void ALKitBindingRegistry::add(LLView* root, const std::string& widget, Factory f)
{
    for (auto i = mFactories.begin(); i != mFactories.end();)
        if (i->second.owner.isDead())
            i = mFactories.erase(i);
        else
            ++i;
    if (root)
    {
        ALKitPage* page = dynamic_cast<ALKitPage*>(root);
        if (!page)
        {
            std::function<ALKitPage*(LLView*)> find_page;
            find_page = [&find_page](LLView* view) -> ALKitPage*
            {
                for (LLView* child : *view->getChildList())
                {
                    if (ALKitPage* found = dynamic_cast<ALKitPage*>(child))
                        return found;
                    if (ALKitPage* found = find_page(child))
                        return found;
                }
                return nullptr;
            };
            page = find_page(root);
        }
        if (page)
            mFactories[bindingKey(page, widget)] = { page->getHandle(), f };
        mFactories[bindingKey(root, widget)] = { root->getHandle(), std::move(f) };
        if (page)
            page->rebindRows();
    }
}

std::unique_ptr<ALKitBinding> ALKitBindingRegistry::make(LLView* root, const std::string& widget) const
{
    for (auto i = mFactories.begin(); i != mFactories.end();)
        if (i->second.owner.isDead())
            i = mFactories.erase(i);
        else
            ++i;
    auto found = mFactories.find(bindingKey(root, widget));
    return found == mFactories.end() ? nullptr : found->second.factory();
}

boost::signals2::connection ALKitSettingWatch::watch(const std::string& name, Callback cb)
{
    auto& node = mNodes[name];
    if (!node)
    {
        node = std::make_unique<Node>();
        if (LLControlVariable* c = gSavedSettings.getControl(name).get())
        {
            Node* n   = node.get();
            n->source = c->getSignal()->connect([n](LLControlVariable*, const LLSD& value, const LLSD& old) { n->signal(value, old); });
        }
    }
    return node->signal.connect(std::move(cb));
}

F64 ALKitSettingWatch::debugRecomputeAll() const
{
    LLTimer timer;
    for (const auto& entry : mNodes)
        if (LLControlVariable* c = gSavedSettings.getControl(entry.first).get())
        {
            const LLSD value = c->get();
            entry.second->signal(value, value);
        }
    return timer.getElapsedTimeF64() * 1000.;
}

static std::unordered_map<std::string, ALKitWhoAdapters::Adapter>& whoMap()
{
    static std::unordered_map<std::string, ALKitWhoAdapters::Adapter> m;
    return m;
}

void ALKitWhoAdapters::add(const std::string& key, Adapter a)
{ whoMap()[key] = std::move(a); }
ALKitWhoAdapters::Adapter ALKitWhoAdapters::get(const std::string& key)
{
    auto i = whoMap().find(key);
    return i == whoMap().end() ? Adapter() : i->second;
}

namespace
{
struct ModeAdapterEntry
{
    LLHandle<LLView> owner;

    ALKitModeAdapters::Adapter adapter;
};
} // namespace

static std::unordered_map<std::string, ModeAdapterEntry>& modeMap()
{
    static std::unordered_map<std::string, ModeAdapterEntry> m;
    return m;
}

void ALKitModeAdapters::add(LLView* owner, const std::string& key, Adapter a)
{
    for (auto i = modeMap().begin(); i != modeMap().end();)
        if (i->second.owner.isDead())
            i = modeMap().erase(i);
        else
            ++i;
    if (owner)
        modeMap()[llformat("%p:%s", owner, key.c_str())] = { owner->getHandle(), std::move(a) };
}
ALKitModeAdapters::Adapter ALKitModeAdapters::get(LLView* owner, const std::string& key)
{
    for (auto i = modeMap().begin(); i != modeMap().end();)
        if (i->second.owner.isDead())
            i = modeMap().erase(i);
        else
            ++i;
    auto i = modeMap().find(llformat("%p:%s", owner, key.c_str()));
    return i == modeMap().end() ? Adapter() : i->second.adapter;
}
