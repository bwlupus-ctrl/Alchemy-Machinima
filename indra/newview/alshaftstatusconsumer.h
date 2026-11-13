/** Draw-path heartbeat: hidden, minimized, closed and clipped consumers expire
 * without visibility notifications or assumptions about lazy-page attachment.
 */
#ifndef AL_SHAFT_STATUS_CONSUMER_H
#define AL_SHAFT_STATUS_CONSUMER_H
namespace ALShaftStatus
{
inline unsigned lastDrawFrame = 0;
inline bool hasDrawn = false;
inline void markDrawn(unsigned frame)
{
    lastDrawFrame = frame;
    hasDrawn = true;
}
// UI and world passes can run in either order. Accept only this/previous frame.
inline bool recentlyDrawn(unsigned frame)
{
    return hasDrawn && frame - lastDrawFrame <= 1u;
}
}
#endif
