/**
 * @file llprismlens.h
 * @brief Phase 0 projection/target spike for the Prism Lens feature.
 */

#ifndef LL_LLPRISMLENS_H
#define LL_LLPRISMLENS_H

namespace LLPrismLens
{
// Render the fixed off-axis debug view before the main scene stateSort.
void renderAuxiliaryView();

// Copy the auxiliary linear-HDR beauty into the main scene debug rectangle.
void compositeDebug();
}

#endif // LL_LLPRISMLENS_H
