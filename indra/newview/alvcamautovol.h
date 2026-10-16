/**
 * @file alvcamautovol.h
 * @brief Pure ownership policy for temporary VCam volumetric overrides.
 */

#ifndef AL_VCAM_AUTO_VOL_H
#define AL_VCAM_AUTO_VOL_H

namespace ALVCamAutoVol
{
// Kept with a viewer-local override, never serialized into an object or scene.
// A user edit relinquishes restoration even after an off/on round trip.
struct EnableOverride
{
    unsigned mUsers = 0;
    bool mAutoEnabled = false;
    bool mDeclined = false;

    // Returns whether the caller must enable the flag. Additional bindings
    // share ownership without overriding an intervening user choice.
    constexpr bool acquire(bool enabled, bool allowed = true)
    {
        mDeclined = !enabled && (mDeclined || !allowed);
        if (mUsers++ != 0) return false;
        mAutoEnabled = !enabled && !mDeclined;
        return mAutoEnabled;
    }

    constexpr void changed() { mAutoEnabled = false; }
    constexpr void changed(bool enabled)
    {
        mDeclined = !enabled;
        changed();
    }

    // Returns whether the last binding must restore the original off state.
    constexpr bool release(bool enabled)
    {
        if (mUsers == 0 || --mUsers != 0) return false;
        const bool restore = mAutoEnabled && enabled;
        mAutoEnabled = false;
        return restore;
    }
};
} // namespace ALVCamAutoVol

#endif // AL_VCAM_AUTO_VOL_H
