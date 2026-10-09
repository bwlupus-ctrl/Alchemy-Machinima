/**
 * @file alsettingsundohistory.h
 * @brief Session-only undo/redo for the shared exposure settings.
 */

#ifndef AL_SETTINGS_UNDO_HISTORY_H
#define AL_SETTINGS_UNDO_HISTORY_H

#include "alsettingsundohistorymodel.h"

#include <boost/signals2.hpp>

#include <cstddef>
#include <string>
#include <vector>

class LLView;

namespace ALExposureSettingsHistory
{
    struct TrackedSetting
    {
        const char* mKey;
        const char* mLabel;
    };

    // The single source of truth for settings recorded by this history.
    const TrackedSetting* trackedSettings();
    std::size_t trackedSettingCount();
    const TrackedSetting* findTrackedSetting(const std::string& key);

    void initialize();
    void clear();
    void beginEdit();
    void endEdit();
    void installEditFinalizers(LLView* root);
    bool canUndo();
    bool canRedo();
    void undo();
    void redo();
    std::string undoLabel();
    std::string redoLabel();
    U64 revision();

    using revision_signal_t = boost::signals2::signal<void(U64)>;
    boost::signals2::connection connectRevisionChanged(
        const revision_signal_t::slot_type& slot);

    class ScopedBatch
    {
    public:
        explicit ScopedBatch(const std::string& label);
        ~ScopedBatch();
        ScopedBatch(const ScopedBatch&) = delete;
        ScopedBatch& operator=(const ScopedBatch&) = delete;
    };
}

#endif // AL_SETTINGS_UNDO_HISTORY_H
