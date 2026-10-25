/**
 * @file alsensormodel.h
 * @brief Pure, illustrative EMVA-style sensor calibration; no renderer/settings.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_SENSOR_MODEL_H
#define AL_SENSOR_MODEL_H

#include <array>
#include <cstdint>

namespace ALSensorModel
{
    using Matrix = std::array<std::array<double, 3>, 3>; // row-major
    enum class BrightnessMode { Neutral, Brightens };
    enum class GainMode { LCG, HCG };

    struct Calibration
    {
        double fullWell;
        double readNoise;
        double analogBase; // DN/e-, multiplied by ISO / baseISO in BOTH modes
        double pedestal;
        double white;
    };

    struct Preset
    {
        const char* name;
        double baseISO;
        double switchISO; // zero: single conversion gain
        double headroom;
        Calibration lcg;
        Calibration hcg;
        Matrix cameraToWorking;
    };

    struct Parameters
    {
        GainMode mode;
        Calibration calibration;
        double iso;
        double gain;
        double neutralGain;
        double analogGain;
        double saturation;
        double electronsPerLinear; // K: fixed at base, never reset at switch
        double headroomStops;
        double noiseFloorLinear; // read + ADC RMS, in output linear units
        double noiseFloorStops; // log2(0.18 / noiseFloorLinear)
        double dynamicRangeStops; // binding oracle: log2(e_sat / sigma_r)
    };

    // std140-compatible layout for GLSL SensorParameters (matrices are padded
    // COLUMN vectors). Also usable with ordinary struct-uniform field uploads.
    struct alignas(16) Uniforms
    {
        std::array<float, 4> physics; // FW, sigma_r, a_m, B
        std::array<float, 4> scale; // W, K, g_N, knee/e_sat
        std::array<float, 4> controls; // noise amount, chroma amount, total gain, green read RMS
        std::array<std::array<float, 4>, 3> cameraToWorking;
        std::array<std::array<float, 4>, 3> workingToCamera;
        std::array<std::uint32_t, 4> clock; // frame low, frame high, seed, stream
        std::array<std::array<float, 4>, 3> readCameraBasis; // fixed C*luma/chroma basis, in camera space
        std::array<std::uint32_t, 4> randomContext; // identical integer prefix, reserved lanes zero
    };

    // These are illustrative published-CLASS values, not measured camera profiles.
    const std::array<Preset, 3>& illustrativePresets();
    Parameters derive(const Preset&, double iso,
                      BrightnessMode = BrightnessMode::Neutral);
    Matrix covariance(const Matrix& cameraToWorking,
                      const std::array<double, 3>& cameraVariance);
    // Positive semidefinite matrices are accepted (including zero noise).
    Matrix cholesky(const Matrix& sigma);
    // Refresh the capture clock AND its cached integer hashes together.
    void setClock(Uniforms&, std::uint64_t frame, std::uint32_t seed, std::uint32_t stream);
    Uniforms pack(const Preset&, const Parameters&, std::uint64_t frame = 0,
                  std::uint32_t seed = 0, std::uint32_t stream = 0,
                  float noiseAmount = 1.f, float chromaAmount = 1.f);
}
#endif
