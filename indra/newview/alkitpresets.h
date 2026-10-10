#ifndef AL_KIT_PRESETS_H
#define AL_KIT_PRESETS_H
#include "llpanel.h"
#include "llsingleton.h"
#include "llsd.h"
#include <boost/signals2.hpp>
#include <memory>
#include <string>
#include <vector>
class ALKitPresetProvider
{
public:
    struct Entry
    {
        std::string id, label;
        bool        builtin = false;
    };
    virtual ~ALKitPresetProvider()                                          = default;
    virtual const std::string&              key() const                     = 0;
    virtual void                            list(std::vector<Entry>&) const = 0;
    virtual bool                            apply(const std::string&)       = 0;
    virtual bool                            canSaveUser() const { return false; }
    virtual bool                            saveUser(const std::string&) { return false; }
    virtual bool                            canDelete(const std::string&) const { return false; }
    virtual bool                            deleteUser(const std::string&) { return false; }
    virtual U64                             revision() const        = 0;
    virtual const std::vector<std::string>& trackedSettings() const = 0;
};
class ALKitPresetRegistry : public LLSingleton<ALKitPresetRegistry>
{
    LLSINGLETON_EMPTY_CTOR(ALKitPresetRegistry);

public:
    void                 add(std::unique_ptr<ALKitPresetProvider>);
    ALKitPresetProvider* get(const std::string&);

private:
    std::vector<std::unique_ptr<ALKitPresetProvider>> mProviders;
};
class ALKitSnapshotStore
{
public:
    ALKitSnapshotStore(std::string, std::vector<std::string>);
    std::vector<std::string> names() const;
    bool                     save(const std::string&);
    bool                     load(const std::string&);
    bool                     remove(const std::string&);
    U64                      revision() const { return mRevision; }

private:
    bool                     read(LLSD&) const;
    bool                     write(const LLSD&) const;
    bool                     ensureLoaded() const;
    std::string              path() const;
    std::string              mKey;
    std::vector<std::string> mSettings;
    U64                      mRevision = 0;
    mutable LLSD             mCache;
    mutable bool             mLoaded    = false;
    mutable bool             mLoadValid = false;
};

class LLButton;
class LLComboBox;
class LLLineEditor;

class ALKitPresetStrip final : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Mandatory<std::string> provider;
        Mandatory<std::string> page;
        Params();
    };
    bool postBuild() override;
    void refresh();

protected:
    friend class LLUICtrlFactory;
    explicit ALKitPresetStrip(const Params& params);

private:
    void                                            onSelect();
    void                                            showSaveEditor();
    void                                            save();
    void                                            remove();
    void                                            captureApplied(const std::string& id, const std::string& label);
    void                                            trackedSettingChanged();
    bool                                            isModified() const;
    std::string                                     mProviderKey;
    std::string                                     mPage;
    std::string                                     mAppliedId;
    std::string                                     mAppliedLabel;
    ALKitPresetProvider*                            mProvider = nullptr;
    LLComboBox*                                     mCombo    = nullptr;
    LLLineEditor*                                   mName     = nullptr;
    LLButton*                                       mSave     = nullptr;
    LLButton*                                       mDelete   = nullptr;
    std::vector<LLSD>                               mAppliedValues;
    std::vector<boost::signals2::scoped_connection> mConnections;
    bool                                            mApplying = false;
    bool                                            mModified = false;
};
#endif
