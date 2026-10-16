/**
 * @file alsettingsundohistorymodel.h
 * @brief Pure model for a bounded, coalescing settings undo history.
 */

#ifndef AL_SETTINGS_UNDO_HISTORY_MODEL_H
#define AL_SETTINGS_UNDO_HISTORY_MODEL_H

#include "llsd.h"
#include "stdtypes.h"

#include <cstddef>
#include <string>
#include <vector>

class ALSettingsUndoHistoryModel
{
public:
    struct Change
    {
        std::string mKey;
        std::string mLabel;
        LLSD        mOldValue;
        LLSD        mNewValue;
    };

    struct Step
    {
        std::string         mLabel;
        U64                 mSequence = 0;
        std::vector<Change> mChanges;
    };

    static constexpr U64 COALESCE_MILLISECONDS = 500;
    static constexpr std::size_t MAX_DEPTH = 100;

    void record(const std::string& key, const std::string& label,
                const LLSD& old_value, const LLSD& new_value, U64 now_ms);
    void beginBatch(const std::string& label);
    void endBatch();
    void beginEdit();
    void endEdit();
    void closeIdle(U64 now_ms);
    void clear();
    void discardRedo();

    bool canUndo() const;
    bool canRedo() const;
    bool takeUndo(Step& step);
    bool takeRedo(Step& step);

    std::string undoLabel() const;
    std::string redoLabel() const;
    U64 revision() const { return mRevision; }
    U64 stepSequence(bool redo) const;
    bool isEditActive() const { return mEditActive; }

private:
    void commitPending();
    void pushUndo(Step&& step, bool clear_redo);
    static void mergeChange(Step& step, const Change& change);
    void changed();

    std::vector<Step> mUndo;
    std::vector<Step> mRedo;
    Step              mPending;
    Step              mBatch;
    U64               mLastChangeMs = 0;
    U64               mRevision = 1;
    U32               mBatchDepth = 0;
    bool              mEditActive = false;
};

#endif // AL_SETTINGS_UNDO_HISTORY_MODEL_H
