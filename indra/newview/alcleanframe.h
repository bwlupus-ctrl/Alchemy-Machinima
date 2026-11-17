#ifndef AL_CLEAN_FRAME_H
#define AL_CLEAN_FRAME_H
class ALKeyframePlayer;
class ALCleanFrame
{
public:
    static bool active() { return sActive; }
private:
    friend class ALKeyframePlayer; // only the transport lease may change this
    static void setActive(bool active) { sActive = active; }
    inline static bool sActive = false;
};
#endif
