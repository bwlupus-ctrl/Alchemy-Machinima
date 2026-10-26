/**
 * @file alsensormodel_test.cpp
 * @brief Pure sensor calibration, gain continuity and covariance tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alsensormodel.h"
#include <cmath>
#include <cstddef>
#include <limits>

namespace tut
{
    using namespace ALSensorModel;
    struct sensor_model_data {};
    typedef test_group<sensor_model_data> sensor_model_group;
    typedef sensor_model_group::object sensor_model_object;
    sensor_model_group sensor_model_tests("ALSensorModel");

    namespace
    {
        void close(const char* label, double a, double b, double eps = 1e-9)
        {
            ensure(label, std::abs(a-b) <= eps * (1. + std::abs(b)));
        }
    }

    template<> template<>
    void sensor_model_object::test<1>()
    {
        for (const auto& preset : illustrativePresets())
        {
            const auto p = derive(preset, preset.baseISO);
            close("base headroom defines K", p.headroomStops, preset.headroom);
            close("K from base saturation", p.electronsPerLinear,
                  p.saturation / (0.18 * std::exp2(preset.headroom)));
            close("K fixed through all gain modes", derive(preset,25600.).electronsPerLinear, p.electronsPerLinear);
            for (auto mode : {BrightnessMode::Neutral, BrightnessMode::Brightens})
                for (double iso : {preset.baseISO,3200.,25600.})
                {
                    const auto q = derive(preset,iso,mode);
                    const double floor = std::sqrt(q.calibration.readNoise*q.calibration.readNoise +
                        1./(12.*q.analogGain*q.analogGain)) * (iso/preset.baseISO) / q.electronsPerLinear;
                    close("noise floor includes total gain and quantisation",q.noiseFloorLinear,floor);
                    close("noise floor stops",q.noiseFloorStops,std::log2(.18/floor));
                    close("dynamic range at non-base ISO",q.dynamicRangeStops,
                          std::log2(q.saturation/q.calibration.readNoise));
                }
        }
    }

    template<> template<>
    void sensor_model_object::test<2>()
    {
        const auto& preset = illustrativePresets()[1];
        for (auto mode : {BrightnessMode::Neutral, BrightnessMode::Brightens})
        {
            const auto before = derive(preset, std::nextafter(preset.switchISO, 0.), mode);
            const auto after = derive(preset, preset.switchISO, mode);
            ensure("LCG below switch", before.mode == GainMode::LCG);
            ensure("HCG at switch", after.mode == GainMode::HCG);
            close("total ISO gain continuous", before.gain, after.gain);
            close("collection scale continuous", before.neutralGain, after.neutralGain);
            // Analog DN/e- can step, but decoding by that SAME a cancels it.
            const double incident = .002;
            const double collectedBefore = before.electronsPerLinear * incident / before.neutralGain;
            const double collectedAfter = after.electronsPerLinear * incident / after.neutralGain;
            close("brightness does not reset", before.analogGain*collectedBefore / before.analogGain * before.gain / before.electronsPerLinear,
                  after.analogGain*collectedAfter / after.analogGain * after.gain / after.electronsPerLinear);
            close("calibrated read-noise step", after.calibration.readNoise / before.calibration.readNoise, .25);
            close("headroom step dictated by saturation", after.headroomStops - before.headroomStops,
                  std::log2(after.saturation / before.saturation));
        }
    }

    template<> template<>
    void sensor_model_object::test<3>()
    {
        for (const auto& preset : illustrativePresets())
            for (auto mode : {BrightnessMode::Neutral, BrightnessMode::Brightens})
                for (double iso : {preset.baseISO*.25, preset.baseISO*.5, preset.baseISO, 12800.})
                {
                    const auto a = derive(preset, iso, mode);
                    const auto b = derive(preset, iso*2., mode);
                    close("general headroom formula", a.headroomStops,
                          std::log2(a.neutralGain*a.saturation / (.18*a.electronsPerLinear)));
                    close("headroom is output clip / output mid-grey", a.headroomStops,
                          std::log2((a.gain*a.saturation/a.electronsPerLinear) / (.18*a.gain/a.neutralGain)));
                    if (a.mode != b.mode) continue;
                    const double fw = a.calibration.fullWell;
                    if (a.saturation == fw && b.saturation == fw)
                        close("FW-limited headroom", b.headroomStops-a.headroomStops,
                              mode == BrightnessMode::Neutral ? 1. : 0.);
                    if (a.saturation < fw && b.saturation < fw)
                        close("ADC-limited headroom", b.headroomStops-a.headroomStops,
                              mode == BrightnessMode::Neutral ? 0. : -1.);
                }
    }

    template<> template<>
    void sensor_model_object::test<4>()
    {
        for (const auto& preset : illustrativePresets())
            for (double iso : {preset.baseISO, 3200., 25600.})
                for (double mu : {0., .5, 12., 20., 1000.})
                {
                    const auto p = derive(preset,iso);
                    const double v = mu + p.calibration.readNoise*p.calibration.readNoise + 1./(12.*p.analogGain*p.analogGain);
                    const auto s = covariance(preset.cameraToWorking, {{v, .5*v, v}});
                    const auto c = cholesky(s);
                    for (int r = 0; r < 3; ++r)
                        for (int col = 0; col < 3; ++col)
                        {
                            double reproduced = 0.;
                            for (int k = 0; k < 3; ++k) reproduced += c[r][k]*c[col][k];
                            close("C*C^T reproduces Sigma_rgb", reproduced, s[r][col]);
                        }
                }
    }

    template<> template<>
    void sensor_model_object::test<5>()
    {
        const auto& preset = illustrativePresets()[1];
        const auto p = derive(preset,3200.);
        const auto u = pack(preset,p,0x123456789abcdef0ULL,91,7);
        ensure_equals("frame low", u.clock[0], std::uint32_t(0x9abcdef0));
        ensure_equals("frame high", u.clock[1], std::uint32_t(0x12345678));
        ensure_equals("seed", u.clock[2], std::uint32_t(91));
        ensure_equals("stream", u.clock[3], std::uint32_t(7));
        ensure_equals("matrix offset std140", offsetof(Uniforms,cameraToWorking), std::size_t(48));
        ensure_equals("inverse offset std140", offsetof(Uniforms,workingToCamera), std::size_t(96));
        ensure_equals("clock offset std140", offsetof(Uniforms,clock), std::size_t(144));
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
            {
                close("column packing", u.cameraToWorking[c][r], preset.cameraToWorking[r][c], 1e-7);
                double value = 0.;
                for (int k = 0; k < 3; ++k) value += u.cameraToWorking[k][r]*u.workingToCamera[c][k];
                close("packed inverse", value, r == c ? 1. : 0., 1e-6);
            }
        const auto below = derive(preset,preset.baseISO*.5);
        ensure("below base is valid", below.gain == .5);
        close("nonfinite ISO uses base", derive(preset,std::numeric_limits<double>::quiet_NaN()).iso,preset.baseISO);
    }

    template<> template<>
    void sensor_model_object::test<6>()
    {
        for (const auto& preset : illustrativePresets())
            for (double iso : {preset.baseISO,3200.,25600.})
            {
                const auto p = derive(preset,iso);
                const auto u = pack(preset,p);
                Matrix workingBasis{};
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 3; ++c)
                        for (int k = 0; k < 3; ++k)
                            workingBasis[r][c] += preset.cameraToWorking[r][k]*u.readCameraBasis[c][k];
                const double v = p.calibration.readNoise*p.calibration.readNoise;
                const auto expected = covariance(preset.cameraToWorking,{{v,v*.5,v}});
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 3; ++c)
                    {
                        double value = 0.;
                        for (int k = 0; k < 3; ++k) value += workingBasis[r][k]*workingBasis[c][k];
                        close("precomputed basis preserves covariance",value,expected[r][c],1e-6);
                    }
                for (int c = 1; c < 3; ++c)
                    close("chroma basis has zero working luminance",.2126*workingBasis[0][c]+.7152*workingBasis[1][c]+.0722*workingBasis[2][c],0.,1e-6);
                ensure_equals("basis offset std140",offsetof(Uniforms,readCameraBasis),std::size_t(160));
                ensure_equals("context offset std140",offsetof(Uniforms,randomContext),std::size_t(208));
                ensure_equals("uniform size std140",sizeof(Uniforms),std::size_t(224));
            }
    }

    template<> template<>
    void sensor_model_object::test<7>()
    {
        for (const auto& preset : illustrativePresets())
            for (double iso : {preset.baseISO,3200.,25600.})
                for (float chroma : {0.f,.5f,1.f,2.f,4.f})
                {
                    const auto p = derive(preset,iso);
                    const auto u = pack(preset,p,0,0,0,1.f,chroma);
                    const double variance = u.readCameraBasis[0][1]*u.readCameraBasis[0][1] +
                        chroma*chroma*(u.readCameraBasis[1][1]*u.readCameraBasis[1][1] +
                                       u.readCameraBasis[2][1]*u.readCameraBasis[2][1]);
                    close("packed green difference matches average variance",u.controls[3]*u.controls[3],variance,1e-6);
                }
    }

    template<> template<>
    void sensor_model_object::test<9>()
    {
        std::array<Preset, 4> presets = {{illustrativePresets()[0], illustrativePresets()[1],
            illustrativePresets()[2], illustrativePresets()[0]}};
        presets[3].baseISO = 250.;
        presets[3].switchISO = 1600.;
        presets[3].headroom = 4.25;
        presets[3].hcg = {12000., 1.7, .4, 256., 16383.};
        for (const auto& preset : presets)
            for (double iso : {100., 800., 3200., 25600.})
                for (auto mode : {BrightnessMode::Neutral, BrightnessMode::Brightens})
                {
                    const auto p = derive(preset, iso, mode);
                    const std::string shown = readout(preset, iso, mode);
                    ensure("gain readout equals model", shown.substr(0, 3) ==
                        (p.mode == GainMode::HCG ? "HCG" : "LCG"));
                    const auto head = shown.find("Headroom: ") + 10;
                    const auto floor = shown.find("Noise floor: ") + 13;
                    ensure("headroom readout equals model",
                        std::abs(std::stod(shown.substr(head)) - p.headroomStops) <= .00501);
                    ensure("noise floor readout equals model",
                        std::abs(std::stod(shown.substr(floor)) - p.noiseFloorStops) <= .00501);
                }
    }

    template<> template<>
    void sensor_model_object::test<8>()
    {
        const auto& preset = illustrativePresets()[0];
        auto u = pack(preset,derive(preset,100.));
        // Independent literal answers for the shader's ordered uint32 hash.
        setClock(u,0x123456789abcdef0ULL,91,7);
        ensure_equals("prefix known answer",u.randomContext[0],std::uint32_t(513469243));
        const auto packed = pack(preset,derive(preset,100.),0x123456789abcdef0ULL,91,7);
        ensure("pack refreshes clock prefix",packed.randomContext==u.randomContext);
    }
}
