/**
 * @file lluifitdiag.h
 * @brief One-shot development diagnostics for clipped floater content.
 */

#ifndef LL_LLUIFITDIAG_H
#define LL_LLUIFITDIAG_H

class LLFloater;

namespace LLUIFitDiag
{
// Called centrally after the floater's onOpen(), after open-time fitters ran.
// [UIFitNoise] False means a layout animation made this pass incomplete; the caller
// must leave its once-per-instance latch clear so a later open can diagnose again.
bool diagnoseFloater(LLFloater* floater);
}

#endif // LL_LLUIFITDIAG_H
