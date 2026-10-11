#ifndef AL_KIT_BINDING_H
#define AL_KIT_BINDING_H
#include "llsingleton.h"
#include "llhandle.h"
#include "llsd.h"
#include <boost/signals2.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class LLControlVariable;
class LLControlGroup;
class LLUICtrl;
class LLView;
class LLUUID;
class ALKitBinding
{
public:
    virtual ~ALKitBinding()                                = default;
    virtual LLSD                        get() const        = 0;
    virtual LLSD                        getDefault() const = 0;
    virtual void                        set(const LLSD&)   = 0;
    virtual bool                        hasDefault() const { return true; }
    virtual bool                        isDefault(S32 decimals) const;
    virtual boost::signals2::connection connectChanged(std::function<void()> cb) = 0;
    virtual std::string                 describe() const                         = 0;
    virtual void                        reset() { set(getDefault()); }
};
class ALKitSettingBinding final : public ALKitBinding
{
public:
    explicit ALKitSettingBinding(std::string name);
    LLSD                        get() const override;
    LLSD                        getDefault() const override;
    void                        set(const LLSD&) override;
    boost::signals2::connection connectChanged(std::function<void()>) override;
    std::string                 describe() const override { return mName; }

private:
    std::string        mName;
    LLControlVariable* control() const;
};
class ALKitWidgetBinding final : public ALKitBinding
{
public:
    ALKitWidgetBinding(LLUICtrl* ctrl, LLSD def);
    LLSD                        get() const override;
    LLSD                        getDefault() const override { return mDefault; }
    void                        set(const LLSD&) override;
    boost::signals2::connection connectChanged(std::function<void()>) override;
    std::string                 describe() const override { return "Widget"; }

private:
    LLUICtrl* mCtrl;
    LLSD      mDefault;
};
class ALKitSettingsListBinding final : public ALKitBinding
{
public:
    explicit ALKitSettingsListBinding(std::vector<std::string> names) : mNames(std::move(names)) {}
    LLSD                        get() const override;
    LLSD                        getDefault() const override;
    void                        set(const LLSD&) override {}
    void                        reset() override;
    boost::signals2::connection connectChanged(std::function<void()>) override;
    std::string                 describe() const override { return "Settings"; }

private:
    std::vector<std::string>                        mNames;
    boost::signals2::signal<void()>                 mChanged;
    std::vector<boost::signals2::scoped_connection> mSources;
};
class ALKitVecBinding final : public ALKitBinding
{
public:
    ALKitVecBinding(std::string setting, LLUICtrl* x, LLUICtrl* y, LLUICtrl* z = nullptr);
    LLSD                        get() const override;
    LLSD                        getDefault() const override;
    void                        set(const LLSD&) override;
    bool                        isDefault(S32 decimals) const override;
    boost::signals2::connection connectChanged(std::function<void()>) override;
    std::string                 describe() const override { return mSetting; }

private:
    void                                            commit();
    std::string                                     mSetting;
    LLUICtrl*                                       mX;
    LLUICtrl*                                       mY;
    LLUICtrl*                                       mZ;
    bool                                            mSyncing = false;
    std::vector<boost::signals2::scoped_connection> mCommits;
};
class ALKitBindingRegistry : public LLSingleton<ALKitBindingRegistry>
{
    LLSINGLETON_EMPTY_CTOR(ALKitBindingRegistry);

public:
    using Factory = std::function<std::unique_ptr<ALKitBinding>()>;
    void                          add(LLView*, const std::string&, Factory);
    std::unique_ptr<ALKitBinding> make(LLView*, const std::string&) const;

private:
    struct Entry
    {
        LLHandle<LLView> owner;
        Factory          factory;
    };
    mutable std::unordered_map<std::string, Entry> mFactories;
};
class ALKitSettingWatch : public LLSingleton<ALKitSettingWatch>
{
    LLSINGLETON_EMPTY_CTOR(ALKitSettingWatch);

public:
    using Callback = std::function<void(const LLSD&, const LLSD&)>;
    boost::signals2::connection watch(const std::string&, Callback);
    F64                         debugRecomputeAll() const;

private:
    struct Node
    {
        boost::signals2::signal<void(const LLSD&, const LLSD&)> signal;
        boost::signals2::scoped_connection                      source;
    };
    std::unordered_map<std::string, std::unique_ptr<Node>> mNodes;
};
class ALKitWhoAdapters
{
public:
    using Adapter = std::function<LLSD(const LLUUID&)>;
    static void    add(const std::string&, Adapter);
    static Adapter get(const std::string&);
};
class ALKitModeAdapters
{
public:
    enum EOrigin
    {
        USER,
        PASSIVE,
        SEARCH,
        RESTORE
    };
    using Adapter = std::function<bool(bool, EOrigin)>;
    static void    add(LLView*, const std::string&, Adapter);
    static Adapter get(LLView*, const std::string&);
};

// LLControlGroup::setLLSD() accepts only controls declared as TYPE_LLSD.
// Kit metadata carries values as LLSD, so convert to the declared control
// type before writing a typed saved setting.
bool alKitSetTypedControl(LLControlGroup& group, const std::string& name, const LLSD& value);
#endif
