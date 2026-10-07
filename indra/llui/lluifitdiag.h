/**
 * @file lluifitdiag.h
 * @brief One-shot development diagnostics for clipped floater content.
 */

#ifndef LL_LLUIFITDIAG_H
#define LL_LLUIFITDIAG_H

class LLFloater;

namespace LLUIFitDiag
{
// Called centrally after the floater's first onOpen(), after open-time fitters ran.
// The caller owns the once-per-instance latch; this function does no persistent work.
void diagnoseFloater(LLFloater* floater);
}

#endif // LL_LLUIFITDIAG_H
