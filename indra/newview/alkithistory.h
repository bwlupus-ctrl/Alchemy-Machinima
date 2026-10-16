#ifndef AL_KIT_HISTORY_H
#define AL_KIT_HISTORY_H
#include "llhandle.h"
#include "llsd.h"
#include <boost/signals2.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
class LLUICtrl;
class LLView;
class LLControlVariable;
class ALKitHistory
{
public:
    enum EOrigin
    {
        MOUSE,
        WHEEL,
        KEY
    };
    struct Entry
    {
        bool               widget = false;
        std::string        key;
        LLHandle<LLUICtrl> ctrl;
        LLSD               oldv;
        LLSD               newv;
    };
    struct CustomEntry
    {
        std::string                             key;
        std::function<bool(bool, std::string&)> replay;
    };
    struct Step
    {
        std::string              label;
        std::string              context;
        std::string              page;
        U64                      scene_generation = 0;
        std::vector<Entry>       entries;
        std::vector<CustomEntry> custom;
    };
    static constexpr size_t MAX_STEPS = 200;

    ALKitHistory();
    ~ALKitHistory();
    void        setOwner(LLView* owner);
    void        beginGesture(EOrigin, const std::string&, const std::string& label = std::string());
    static void noteKey();
    static void notePointer(LLView*);
    void        watchSettings(const std::string&, const std::vector<std::string>&);
    void        recordWidget(LLUICtrl*, const LLSD&, const LLSD&, const std::string&);
    void        recordCustom(LLView*, const std::string&, const std::string&, std::function<bool(bool, std::string&)>);
    void        barrier(const std::string&);
    static void globalBarrier(const std::string&);
    bool        canUndo(const std::string& page) const;
    bool        canRedo(const std::string& page) const;
    bool        canUndo() const { return !mUndo.empty(); }
    void        undo(const std::string& page);
    void        redo(const std::string& page);
    U64         revision() const { return mRevision; }
    void        registerContext(const std::string&, std::function<std::string()>);
    struct ScopedSuspend
    {
        ScopedSuspend();
        ~ScopedSuspend();
    };
    static bool suspended();

private:
    class GestureTimer;
    void                          captureSetting(const std::string&, const std::string&, const LLSD&, const LLSD&);
    void                          addEntry(Entry, const std::string&, const std::string& label_page = std::string());
    void                          finishGesture();
    bool                          tickGesture();
    std::string                   currentContext(const std::string&) const;
    bool                          replay(Step&, bool);
    std::vector<Step>             mUndo;
    std::vector<Step>             mRedo;
    U64                           mRevision   = 0;
    U64                           mGeneration = 0;
    std::string                   mPage;
    std::string                   mGestureLabel;
    EOrigin                       mOrigin = MOUSE;
    LLHandle<LLView>              mOwner;
    std::unique_ptr<Step>         mOpen;
    std::unique_ptr<GestureTimer> mTimer;
    F64                           mCloseAt   = 0.;
    bool                          mReplaying = false;
    std::unordered_map<std::string, std::vector<boost::signals2::scoped_connection>> mWatches;
    std::unordered_map<std::string, std::function<std::string()>>                    mContexts;
};
#endif
