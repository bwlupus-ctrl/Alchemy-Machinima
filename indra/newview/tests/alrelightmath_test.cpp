#include "linden_common.h"
#include "../test/lltut.h"
#include "../alrelightmath.h"
#include <limits>
namespace tut
{
struct relight_data {};
using relight_group = test_group<relight_data>;
using relight_object = relight_group::object;
relight_group relight_tests("alrelightmath");
static void close(const char* label, double a, double b, double tolerance = 1.e-10)
{ ensure(label, std::isfinite(a) && std::abs(a-b) <= tolerance); }
template<> template<> void relight_object::test<1>()
{
    // R=15, f=.5: legacy(7.5)=2/9; physical(7.5)=1/64, gain=128/9.
    close("literal legacy",ALRelight::legacy(7.5,15,.5),2.0/9.0);
    close("literal physical",ALRelight::physical(7.5,15,.3,.1),1.0/64.0);
    close("literal gain",ALRelight::gain(7.5,15,.5,.3,.1),128.0/9.0);
    for (double reference : {.05,.3,1.5,7.5,14.9})
    {
        double gain = ALRelight::gain(reference,15,.5,.3,.1);
        close("calibration",gain*ALRelight::physical(reference,15,.3,.1),ALRelight::legacy(reference,15,.5));
    }
}
template<> template<> void relight_object::test<2>()
{
    double previous = std::numeric_limits<double>::infinity();
    for (int i=0; i<=16000; ++i)
    {
        const double d = i*.001;
        const double value = ALRelight::capped(d,15,.5,.3,.1,128.0/9.0,4);
        ensure("finite monotonic falloff",std::isfinite(value) && value <= previous+1.e-12);
        if (d >= 15) close("range cutoff",value,0);
        previous=value;
    }
}
template<> template<> void relight_object::test<3>()
{
    for (double radius : {0.0,.001,.3,2.0})
        for (double floor : {0.0,.0001,.1,1.0})
            for (double boost : {0.0,.25,4.0,100.0})
                for (double d : {0.0,1.e-12,.0001,.05,.1,1.0})
                {
                    const double value = ALRelight::capped(d,15,.5,radius,floor,128.0/9.0,boost);
                    ensure("near bound",std::isfinite(value) && value >= 0 && value <= boost*(8.0/9.0)+1.e-12);
                }
    close("zero range",ALRelight::physical(0,0,0,0),0);
    close("reference outside range",ALRelight::gain(16,15,.5,.3,.1),0);
}
template<> template<> void relight_object::test<4>()
{
    // Eq. 10's divisor 3 is discriminated from 2 by this literal.
    close("Karis constant",ALRelight::normalization(.25,.6,2),25.0/49.0);
    double raw=0, normalized=0;
    for (int a=1;a<=100;++a)
        for (int h=0;h<=100;++h)
        {
            const double alpha=a*.01, cosine=h*.01;
            const double denom=cosine*cosine*(alpha*alpha-1)+1;
            const double lobe=alpha*alpha/(3.141592653589793*denom*denom);
            const double norm=ALRelight::normalization(alpha,.3,2);
            ensure("normalization never boosts",norm>=0 && norm<=1.0);
            raw+=lobe; normalized+=lobe*norm;
        }
    ensure("hemisphere normalization lowers integrated representative lobe",normalized<raw);
    close("point normalization",ALRelight::normalization(.25,0,2),1);
}
template<> template<> void relight_object::test<5>()
{
    using namespace ALRelight;
    const Vec n{0,0,1}, v{0,0,1}, light{1,0,2};
    const Vec rep=representative(n,v,light,.5);
    close("toward ray x",rep[0],.5/std::sqrt(4.25));
    close("toward ray z",rep[2],2/std::sqrt(4.25));
    const Vec point=representative(n,v,light,0);
    close("point direction",point[0],1/std::sqrt(5.0));
    const Vec limit=representative(n,v,light,1.e-8);
    close("small source limit",limit[0],point[0],1.e-8);
    const Vec inside=representative(n,v,Vec{0,0,0},1);
    close("finite centre",inside[2],1);
}
template<> template<> void relight_object::test<6>()
{
    for (double cosine : {-1.0,-.2,0.0,.2,1.0})
    {
        close("point exact legacy angular term",ALRelight::diffuseRatio(cosine,0,10),std::max(cosine,0.0),0);
        close("small source ratio limit",ALRelight::diffuseRatio(cosine,1.e-5,10),std::max(cosine,0.0),1.e-6);
        close("inside hemisphere",ALRelight::diffuseRatio(cosine,1,.5),1);
    }
    ensure("horizon wrap survives",ALRelight::diffuseRatio(-.1,1,2)>0);
    close("behind cap dark",ALRelight::diffuseRatio(-1,1,2),0);
    close("above horizon ratio",ALRelight::diffuseRatio(.8,1,2),.8);
    // Entire source below (or tangent to) the surface horizon: exactly dark.
    for (double radius : {.005,.3,1.0})
        for (double cosine : {-1.0,-.8,-.5})
            close("legacy fully hidden sphere diffuse",ALRelight::diffuseRatio(cosine,radius,2),0,0);

}
template<> template<> void relight_object::test<7>()
{
    // Independent numerical solid-angle integral over a cap, including horizon.
    constexpr double pi=3.14159265358979323846;
    for (double cosine : {-.7,-.2,0.0,.2,.7})
    {
        const double sigma=std::asin(.5), cosSigma=std::cos(sigma);
        double integral=0;
        for (int t=0;t<300;++t)
            for (int p=0;p<600;++p)
            {
                double ct=1-(1-cosSigma)*(t+.5)/300;
                double st=std::sqrt(1-ct*ct), phi=2*pi*(p+.5)/600;
                integral+=std::max(cosine*ct+std::sqrt(1-cosine*cosine)*st*std::cos(phi),0.0);
            }
        integral*=(1-cosSigma)*2*pi/(300*600);
        close("sphere integral matches quadrature",ALRelight::sphere(cosine,.25),integral,2.e-5);
    }
}
template<> template<> void relight_object::test<8>()
{
    // Forward's diffuse clamp must survive the small-source mode-0 limit.
    for (double cosine : {.2,.8,1.0})
    {
        const double attenuation=ALRelight::legacy(.1,15,0);
        const double point=std::clamp(cosine*attenuation,0.0,1.0);
        const double shaped=std::clamp(ALRelight::diffuseRatio(cosine,1.e-5,.1)*attenuation,0.0,1.0);
        close("forward small-source diffuse clamp",shaped,point,1.e-6);
    }
    // A single CPU calibration also matches forward's algebraic legacy window.
    for (double f : {0.0,.25,.5,1.0})
        for (double d : {0.0,.1,1.5,7.5,15.0,16.0})
        {
            const double fa=1+f;
            const double a=std::clamp(1.0-(d/15-(1-fa))/fa,0.0,1.0);
            close("forward/deferred window",2*a*a,ALRelight::legacy(d,15,f));
        }
}
}
