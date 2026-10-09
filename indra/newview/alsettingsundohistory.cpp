/**
 * @file alsettingsundohistory.cpp
 * @brief Session-only undo/redo for the shared exposure settings.
 */

#include "llviewerprecompiledheaders.h"

#include "alsettingsundohistory.h"

#include "llbutton.h"
#include "llcontrol.h"
#include "lleventtimer.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltimer.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace
{
const ALExposureSettingsHistory::TrackedSetting TRACKED_SETTINGS[] =
{
    { "AlchemyEnvSunEV",               "Sun EV" },
    { "AlchemyEnvSkyGIEV",             "Sky light (GI) EV" },
    { "AlchemyEnvMoonLinked",          "Link Moon to Sun" },
    { "AlchemyEnvMoonEV",              "Moon EV" },
    { "AlchemyEnvGIAmbientEV",         "Sky ambient EV" },
    { "AlchemyEnvGIProbeDiffuseEV",    "Probe diffuse EV" },
    { "AlchemyEnvGIProbeSpecEV",       "Probe reflections EV" },
    { "AlchemyEnvLocalLightEV",        "Local lights EV" },
    { "AlchemyEnvLocalLightIncludeRig", "Include rig lights" },
    { "AlchemyEnvSunKelvin",           "Sun temperature" },
    { "AlchemyEnvSunTintColor",        "Sun tint" },
    { "AlchemyEnvSunTintStrength",     "Sun tint strength" },
    { "AlchemyEnvMoonTintColor",       "Moon tint" },
    { "AlchemyEnvMoonTintStrength",    "Moon tint strength" },
    { "AlchemyEnvAmbientTintColor",    "Ambient tint" },
    { "AlchemyEnvAmbientTintStrength", "Ambient tint strength" },
    { "AlchemyEnvShadowLiftEV",        "Shadow lift EV" },
    { "RenderDynamicExposureEnabled",  "Auto exposure" },
    { "RenderExposure",                "Camera exposure" },
};

U64 nowMilliseconds()
{
    return static_cast<U64>(static_cast<F64>(LLTimer::getTotalSeconds()) * 1000.0);
}

class HistoryService final : public LLEventTimer
{
public:
    HistoryService()
    :   LLEventTimer(0.25f)
    {
        for (const auto& setting : TRACKED_SETTINGS)
        {
            if (LLControlVariable* control = gSavedSettings.getControl(setting.mKey))
            {
                mConnections.emplace_back(control->getSignal()->connect(
                    [this, key = std::string(setting.mKey), label = std::string(setting.mLabel)](
                        LLControlVariable*, const LLSD& new_value, const LLSD& old_value)
                    {
                        if (!mApplying)
                        {
                            mModel.record(key, label, old_value, new_value, nowMilliseconds());
                            publishRevision();
                        }
                    }));
            }
            else
            {
                LL_WARNS("ExposureHistory") << "Missing tracked setting "
                                             << setting.mKey << LL_ENDL;
            }
        }
    }

    bool tick() override
    {
        if (mModel.isEditActive() && !gFocusMgr.getMouseCapture())
        {
            endEdit();
        }
        const U64 before = mModel.revision();
        mModel.closeIdle(nowMilliseconds());
        if (mModel.revision() != before)
        {
            publishRevision();
        }
        return false;
    }

    void beginBatch(const std::string& label)
    {
        mModel.beginBatch(label);
    }

    void endBatch()
    {
        const U64 before = mModel.revision();
        mModel.endBatch();
        if (mModel.revision() != before)
        {
            publishRevision();
        }
    }

    void endEdit()
    {
        const U64 before = mModel.revision();
        mModel.endEdit();
        if (mModel.revision() != before)
        {
            publishRevision();
        }
    }

    void beginEdit()
    {
        const U64 before = mModel.revision();
        mModel.beginEdit();
        if (mModel.revision() != before)
        {
            publishRevision();
        }
    }

    void clear()
    {
        const U64 before = mModel.revision();
        mModel.clear();
        if (mModel.revision() != before)
        {
            publishRevision();
        }
    }

    void undo()
    {
        ALSettingsUndoHistoryModel::Step step;
        if (mModel.takeUndo(step))
        {
            apply(step, false);
        }
    }

    void redo()
    {
        ALSettingsUndoHistoryModel::Step step;
        if (mModel.takeRedo(step))
        {
            apply(step, true);
        }
    }

    ALSettingsUndoHistoryModel& model() { return mModel; }
    ALExposureSettingsHistory::revision_signal_t& signal() { return mRevisionSignal; }

private:
    void apply(const ALSettingsUndoHistoryModel::Step& step, bool use_new_value)
    {
        mApplying = true;
        if (use_new_value)
        {
            for (const auto& change : step.mChanges)
            {
                if (LLControlVariable* control = gSavedSettings.getControl(change.mKey))
                {
                    control->setValue(change.mNewValue);
                }
            }
        }
        else
        {
            for (auto change = step.mChanges.rbegin(); change != step.mChanges.rend(); ++change)
            {
                if (LLControlVariable* control = gSavedSettings.getControl(change->mKey))
                {
                    control->setValue(change->mOldValue);
                }
            }
        }
        mApplying = false;
        publishRevision();
    }

    void publishRevision()
    {
        mRevisionSignal(mModel.revision());
    }

    ALSettingsUndoHistoryModel mModel;
    bool mApplying = false;
    std::vector<boost::signals2::scoped_connection> mConnections;
    ALExposureSettingsHistory::revision_signal_t mRevisionSignal;
};

HistoryService& service()
{
    static HistoryService history;
    return history;
}
}

namespace ALExposureSettingsHistory
{
const TrackedSetting* trackedSettings() { return TRACKED_SETTINGS; }
std::size_t trackedSettingCount() { return std::size(TRACKED_SETTINGS); }

const TrackedSetting* findTrackedSetting(const std::string& key)
{
    const auto found = std::find_if(std::begin(TRACKED_SETTINGS), std::end(TRACKED_SETTINGS),
        [&key](const TrackedSetting& setting) { return key == setting.mKey; });
    return found == std::end(TRACKED_SETTINGS) ? nullptr : &*found;
}

void initialize() { (void)service(); }
void clear() { service().clear(); }
void beginEdit() { service().beginEdit(); }
void endEdit() { service().endEdit(); }

void installEditFinalizers(LLView* root)
{
    if (!root)
    {
        return;
    }

    for (auto view = root->beginTreeBFS(); view != root->endTreeBFS(); ++view)
    {
        if (LLSliderCtrl* slider = dynamic_cast<LLSliderCtrl*>(*view))
        {
            slider->setSliderMouseDownCallback(
                [](LLUICtrl*, const LLSD&) { beginEdit(); });
            slider->setSliderMouseUpCallback(
                [](LLUICtrl*, const LLSD&) { endEdit(); });
            slider->setSliderEditorCommitCallback(
                [](LLUICtrl*, const LLSD&) { endEdit(); });
        }
        else if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(*view))
        {
            // Spinner arrows commit repeatedly while held. Finalize the group
            // from release, not from each repeat; typed values finalize on commit.
            for (LLView* child : *spinner->getChildList())
            {
                if (LLButton* button = dynamic_cast<LLButton*>(child))
                {
                    button->setMouseDownCallback(
                        [](LLUICtrl*, const LLSD&) { beginEdit(); });
                    button->setMouseUpCallback(
                        [](LLUICtrl*, const LLSD&) { endEdit(); });
                }
                else if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
                {
                    editor->setCommitCallback(
                        [](LLUICtrl*, const LLSD&) { endEdit(); });
                }
            }
        }
    }
}

bool canUndo() { return service().model().canUndo(); }
bool canRedo() { return service().model().canRedo(); }
void undo() { service().undo(); }
void redo() { service().redo(); }
std::string undoLabel() { return service().model().undoLabel(); }
std::string redoLabel() { return service().model().redoLabel(); }
U64 revision() { return service().model().revision(); }

boost::signals2::connection connectRevisionChanged(const revision_signal_t::slot_type& slot)
{
    return service().signal().connect(slot);
}

ScopedBatch::ScopedBatch(const std::string& label)
{
    service().beginBatch(label);
}

ScopedBatch::~ScopedBatch()
{
    service().endBatch();
}
}
