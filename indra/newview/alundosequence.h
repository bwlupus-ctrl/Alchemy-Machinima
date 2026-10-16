#ifndef AL_UNDO_SEQUENCE_H
#define AL_UNDO_SEQUENCE_H

#include "stdtypes.h"

inline U64& alUndoSequenceClock()
{
    static U64 sequence = 0;
    return sequence;
}

inline U64& alUndoSequenceGroup()
{
    static U64 group = 0;
    return group;
}

// UI histories share one clock; compound actions and their replays share one group.
inline U64 alNextUndoSequence()
{
    return alUndoSequenceGroup() ? alUndoSequenceGroup() : ++alUndoSequenceClock();
}

class ALScopedUndoSequence
{
public:
    ALScopedUndoSequence() : mPrevious(alUndoSequenceGroup())
    {
        if (!mPrevious)
            alUndoSequenceGroup() = ++alUndoSequenceClock();
    }
    ~ALScopedUndoSequence() { alUndoSequenceGroup() = mPrevious; }
    ALScopedUndoSequence(const ALScopedUndoSequence&) = delete;
    ALScopedUndoSequence& operator=(const ALScopedUndoSequence&) = delete;
private:
    U64 mPrevious;
};

#endif
