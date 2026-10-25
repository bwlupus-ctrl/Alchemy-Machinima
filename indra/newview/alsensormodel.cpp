/**
 * @file alsensormodel.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "alsensormodel.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ALSensorModel
{
    namespace
    {
        double saturation(const Calibration& c, double gain)
        {
            return (std::min)(c.fullWell, (c.white - c.pedestal) / gain);
        }

        Matrix inverse(const Matrix& a)
        {
            Matrix cof{};
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    cof[r][c] = a[(r+1)%3][(c+1)%3] * a[(r+2)%3][(c+2)%3]
                              - a[(r+1)%3][(c+2)%3] * a[(r+2)%3][(c+1)%3];
            const double det = a[0][0]*cof[0][0] + a[0][1]*cof[0][1] + a[0][2]*cof[0][2];
            if (!std::isfinite(det) || std::abs(det) < 1e-12)
                throw std::invalid_argument("singular sensor colour matrix");
            Matrix result{};
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    result[r][c] = cof[c][r] / det;
            return result;
        }

        void columns(const Matrix& a, std::array<std::array<float, 4>, 3>& out)
        {
            for (int c = 0; c < 3; ++c)
                for (int r = 0; r < 3; ++r)
                    out[c][r] = static_cast<float>(a[r][c]);
        }

        std::uint32_t sensorHash(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            return value ^ (value >> 16);
        }
    }

    const std::array<Preset, 3>& illustrativePresets()
    {
        // a_base, pedestals, matrices and unspecified H values are explicit
        // illustrative choices, NOT calibrations of any named camera. All have
        // both FW-limited and ADC-limited ISO regions for validation.
        static const Matrix colour = {{{1.08, -0.06, -0.02},
                                       {-0.03, 1.06, -0.03},
                                       {-0.02, -0.08, 1.10}}};
        static const std::array<Preset, 3> presets = {{
            {"Clean full-frame (illustrative)", 100., 0., 3.5,
             {50000., 3., .25, 512., 16383.}, {50000., 3., .25, 512., 16383.}, colour},
            {"Dual-native cinema (illustrative)", 800., 3200., 3.5,
             {40000., 6., .25, 512., 16383.}, {10000., 1.5, .35, 256., 16383.}, colour},
            {"Small / older sensor (illustrative)", 200., 0., 3.,
             // 1024 DN reserves eight read-noise sigmas at ISO 25600; a small
             // fixed pedestal would itself rectify blacks at maximum gain.
             {15000., 5., .20, 1024., 4095.}, {15000., 5., .20, 1024., 4095.}, colour}
        }};
        return presets;
    }

    Parameters derive(const Preset& preset, double iso, BrightnessMode brightness)
    {
        if (!std::isfinite(iso)) iso = preset.baseISO;
        iso = (std::max)(1., (std::min)(iso, 102400.)); // below-base ISO is valid
        Parameters p{};
        p.mode = preset.switchISO > 0. && iso >= preset.switchISO ? GainMode::HCG : GainMode::LCG;
        p.calibration = p.mode == GainMode::HCG ? preset.hcg : preset.lcg;
        const auto& c = p.calibration;
        if (!(preset.baseISO > 0. && c.fullWell > 0. && c.readNoise > 0. &&
              c.analogBase > 0. && c.pedestal >= 0. && c.white > c.pedestal))
            throw std::invalid_argument("invalid sensor calibration");
        p.iso = iso;
        p.gain = iso / preset.baseISO;
        p.neutralGain = brightness == BrightnessMode::Neutral ? p.gain : 1.;
        p.analogGain = c.analogBase * p.gain; // no gain reset at ISO_switch
        p.saturation = saturation(c, p.analogGain);
        p.electronsPerLinear = saturation(preset.lcg, preset.lcg.analogBase) /
                              (0.18 * std::exp2(preset.headroom));
        // Output clip / extrapolated output mid-grey:
        // (g*e_sat/K) / (0.18*g/g_N). Brightens cancels g in this ratio.
        p.headroomStops = std::log2(p.neutralGain * p.saturation / (0.18 * p.electronsPerLinear));
        p.noiseFloorLinear = std::sqrt(c.readNoise*c.readNoise + 1./(12.*p.analogGain*p.analogGain)) *
                             p.gain / p.electronsPerLinear;
        p.noiseFloorStops = std::log2(0.18 / p.noiseFloorLinear);
        p.dynamicRangeStops = std::log2(p.saturation / c.readNoise);
        return p;
    }

    Matrix covariance(const Matrix& m, const std::array<double, 3>& variance)
    {
        Matrix result{};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                for (int k = 0; k < 3; ++k)
                    result[r][c] += m[r][k] * variance[k] * m[c][k];
        return result;
    }

    Matrix cholesky(const Matrix& sigma)
    {
        Matrix result{};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c <= r; ++c)
            {
                double value = sigma[r][c];
                for (int k = 0; k < c; ++k) value -= result[r][k] * result[c][k];
                if (r == c) result[r][c] = std::sqrt((std::max)(0., value));
                else result[r][c] = result[c][c] > 0. ? value / result[c][c] : 0.;
            }
        return result;
    }

    Uniforms pack(const Preset& preset, const Parameters& p, std::uint64_t frame,
                  std::uint32_t seed, std::uint32_t stream, float noiseAmount, float chromaAmount)
    {
        Uniforms u{};
        const auto& c = p.calibration;
        u.physics = {{static_cast<float>(c.fullWell), static_cast<float>(c.readNoise),
                      static_cast<float>(p.analogGain), static_cast<float>(c.pedestal)}};
        u.scale = {{static_cast<float>(c.white), static_cast<float>(p.electronsPerLinear),
                    static_cast<float>(p.neutralGain), .8f}};
        u.controls = {{std::isfinite(noiseAmount) ? (std::max)(0.f, (std::min)(noiseAmount, 4.f)) : 1.f,
                       std::isfinite(chromaAmount) ? (std::max)(0.f, (std::min)(chromaAmount, 4.f)) : 1.f,
                       static_cast<float>(p.gain), 0.f}};
        columns(preset.cameraToWorking, u.cameraToWorking);
        const auto inv = inverse(preset.cameraToWorking);
        columns(inv, u.workingToCamera);
        const double v = c.readNoise * c.readNoise;
        const auto chol = cholesky(covariance(preset.cameraToWorking, {{v, v*.5, v}}));
        // Same orthonormal rotation as sensorGaussian. Read covariance is
        // constant across the image, so factor this once rather than per pixel.
        const std::array<double,3> weights = {{.2126,.7152,.0722}};
        std::array<double,3> axis{};
        double lengthSquared = 0.;
        for (int col = 0; col < 3; ++col)
        {
            for (int row = 0; row < 3; ++row) axis[col] += chol[row][col]*weights[row];
            lengthSquared += axis[col]*axis[col];
        }
        for (auto& value : axis) value /= std::sqrt(lengthSquared);
        std::array<double,3> tangent = std::abs(axis[2]) < .9 ?
            std::array<double,3>{{axis[1],-axis[0],0.}} :
            std::array<double,3>{{0.,axis[2],-axis[1]}};
        const double tangentLength = std::sqrt(tangent[0]*tangent[0]+tangent[1]*tangent[1]+tangent[2]*tangent[2]);
        for (auto& value : tangent) value /= tangentLength;
        const std::array<double,3> bitangent = {{axis[1]*tangent[2]-axis[2]*tangent[1],
            axis[2]*tangent[0]-axis[0]*tangent[2],axis[0]*tangent[1]-axis[1]*tangent[0]}};
        Matrix rotation{}, basis{};
        for (int row = 0; row < 3; ++row)
            rotation[row] = {{axis[row],tangent[row],bitangent[row]}};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                for (int j = 0; j < 3; ++j)
                    for (int k = 0; k < 3; ++k)
                        basis[row][col] += inv[row][j]*chol[j][k]*rotation[k][col];
        columns(basis,u.readCameraBasis);
        const double chroma = u.controls[1];
        u.controls[3] = static_cast<float>(std::sqrt(basis[1][0]*basis[1][0] +
            chroma*chroma*(basis[1][1]*basis[1][1]+basis[1][2]*basis[1][2])));
        setClock(u,frame,seed,stream);
        return u;
    }

    void setClock(Uniforms& u, std::uint64_t frame, std::uint32_t seed, std::uint32_t stream)
    {
        u.clock = {{static_cast<std::uint32_t>(frame), static_cast<std::uint32_t>(frame >> 32), seed, stream}};
        std::uint32_t context = sensorHash(u.clock[0] ^ 0x9e3779b9u);
        for (int word = 1; word < 4; ++word) context = sensorHash(context ^ u.clock[word]);
        u.randomContext = {{context,0u,0u,0u}};
    }

    static_assert(sizeof(Uniforms) == 224, "SensorParameters std140 packing");
}
