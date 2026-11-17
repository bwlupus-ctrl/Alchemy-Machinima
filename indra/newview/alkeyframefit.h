#ifndef AL_KEYFRAME_FIT_H
#define AL_KEYFRAME_FIT_H
#include "alkeyframecore.h"
#include <chrono>
namespace ALKf
{
struct TakeSample { double time = 0; Pose pose; };
// WP3 supplies a read-only LLFlycamRecorder adapter. The core never includes LL headers.
// sample() returns recorded original samples; evaluate() must reproduce recorder interpolation.
class TakeSource
{
public:
    virtual ~TakeSource() = default;
    virtual std::size_t size() const = 0;
    virtual TakeSample sample(std::size_t) const = 0;
    virtual Pose evaluate(double exactTakeTime) const = 0;
};
struct FitTolerance { double position = .02, rotation = .5 * degree, fov = .2 * degree; };
struct FitError { double position = 0, rotation = 0, fov = 0; };
enum class FitVerdict { Pass, Approximate, Refused };
struct FitOptions
{
    FitTolerance tolerance;
    std::size_t maxKeys = 400;
    double maxSeconds = 2;
};
struct FitResult
{
    FitVerdict verdict = FitVerdict::Refused;
    Track track;
    FitError worst;
    std::string reason;
    double elapsedSeconds = 0;
    std::size_t validatedSamples = 0;
};
bool validateTakeHold(const TakeSource&, std::size_t first, std::size_t last, FitTolerance);
FitResult fitTake(const TakeSource&, const FitOptions& = {});
// Commits a NEW identity only. Never replaces an existing track or saved take.
bool appendFit(Sequence&, const FitResult&, std::uint32_t& newTrack);
}
#endif
