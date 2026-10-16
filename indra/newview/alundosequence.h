#ifndef AL_UNDO_SEQUENCE_H
#define AL_UNDO_SEQUENCE_H

#include "stdtypes.h"

// UI histories share one clock, including steps still inside an open gesture.
inline U64 alNextUndoSequence()
{
    static U64 sequence = 0;
    return ++sequence;
}

#endif
