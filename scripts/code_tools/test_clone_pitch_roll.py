#!/usr/bin/env python3
"""Execute production pure orientation math with a standalone C++17 compiler.
Use --compiler cl.exe from a VS developer shell (no viewer build).
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TEST = r'''#include "alghostrotation.h"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace ALGhostRotation;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
double error(const Quaternion& a, const Quaternion& b) {
    double d=0, n=0; for(int i=0;i<4;++i) {d+=(a[i]-b[i])*(a[i]-b[i]); n+=(a[i]+b[i])*(a[i]+b[i]);}
    return std::sqrt(std::min(d,n));
}
void roundtrip(Angles a, bool floats=false) {
    auto q=compose(a); if(floats) for(auto& x:q) x=static_cast<float>(x);
    require(error(q,compose(decompose(q))) < 2.e-6, "unreferenced quaternion round trip");
    auto read=decompose(q,a);
    require(error(q,compose(read)) < 2.e-6, "quaternion round trip");
    for(int i=0;i<3;++i) if (std::fabs(wrap(a[i]-read[i]))>=2.e-5) { std::cerr << "axis " << i << " input " << a[0] << "," << a[1] << "," << a[2] << " read " << read[0] << "," << read[1] << "," << read[2] << "\n"; require(false, "reference branch changed"); }
}
int main() {
 try {
    // Expected independent axis direction: positive side-axis rotation dives +X.
    const auto dive=compose({0,PI/2,0});
    require(std::fabs(dive[1]-std::sqrt(.5))<1.e-12, "pitch sign / side axis");
    const auto bank=compose({0,0,PI/2});
    require(std::fabs(bank[0]-std::sqrt(.5))<1.e-12, "roll / forward axis");
    // A combined orientation must keep forward independent of roll.
    {
    const auto combined = compose({PI/2, PI/6, PI/4});
    const double x=combined[0], y=combined[1], z=combined[2], w=combined[3];
    require(std::fabs(1-2*(y*y+z*z))<1.e-12 &&
            std::fabs(2*(x*y+w*z)-std::sqrt(.75))<1.e-12 &&
            std::fabs(2*(x*z-w*y)+.5)<1.e-12,"intrinsic axis composition order");
    }
    for(double y : {-PI,-2.,0.,.4,PI}) {
      // Old scenes: a yaw-only quaternion must read as zero pitch and roll.
      auto old=decompose(compose({y,0,0}));
      require(std::fabs(old[1])+std::fabs(old[2])<1.e-10,"legacy yaw scene");
      for(double p : {-PI,-PI/2-1.e-4,-PI/2,-PI/2+1.e-4,0.,PI/2-1.e-4,PI/2,PI/2+1.e-4,PI})
       for(double r : {-PI,-.7,0.,.7,PI}) { roundtrip({y,p,r}); roundtrip({y,p,r},true); }
    }
    std::mt19937 gen(42); std::uniform_real_distribution<double> angle(-PI,PI);
    for(int i=0;i<10000;++i) roundtrip({angle(gen),angle(gen),angle(gen)},true);
    for(double direction : {-1.,1.}) {
      Angles reference {.4, direction*80.*PI/180.,.7};
      for(int i=0;i<=2000;++i) {
        Angles requested {.4,direction*(80.+i*.01)*PI/180.,.7};
        auto q=compose(requested); for(auto& v:q) v=static_cast<float>(v);
        auto read=decompose(q,reference);
        require(error(q,compose(read))<2.e-6,"scroll quaternion changed");
        for(int axis=0;axis<3;++axis) require(std::fabs(wrap(read[axis]-requested[axis]))<.002,"gimbal scroll jumped");
        reference=requested; // same successful-commit reference as the panel
      }
    }
    // External singular read-back uses a different cached yaw: the reference
    // fast path cannot hide an incorrect coupled-roll solution.
    for(double sign : {-1.,1.}) {
      const Angles requested {.9, sign*PI/2, -.6};
      const Angles reference {.2, sign*PI/2, .4};
      auto q=compose(requested);
      const auto read=decompose(q,reference);
      require(error(q,compose(read))<1.e-10,"gimbal coupled roll solution");
      require(std::fabs(read[0]-reference[0])<1.e-12,"gimbal yaw reference lost");
      for(auto& v:q) v*=-3.;
      require(error(compose(requested),compose(decompose(q,reference)))<1.e-10,
              "scaled/negated quaternion changed orientation");
    }
    // Shared production heading and relative-edit helpers, including branches
    // past vertical and canonical references after either wrap boundary.
    for (double pitch : {-2.1, -PI/2, .3, PI/2, 2.1}) {
      Angles authored {.52,pitch,-.41}; bool valid=true;
      auto q=compose(authored);
      for (double heading : {-3.,-.2,1.,3.}) {
        q=withHeading(q, heading, authored, valid);
        require(error(q,compose({heading,pitch,-.41}))<1.e-12,"heading erased pitch/roll");
      }
    }
    for(int axis=0;axis<3;++axis) for(double sign : {-1.,1.}) {
      Angles a {.2,.4,.6}; a[axis]=sign*PI;
      auto edited=editAxis(a,axis,sign*(PI+PI/180.),0.,false);
      require(std::fabs(edited[axis]+sign*179.*PI/180.)<1.e-12,"axis wrap boundary");
      auto read=decompose(compose(edited),edited);
      require(std::fabs(read[axis])<=PI,"noncanonical authored reference");
      auto other=a; other[axis]-=.3;
      auto relative=editAxis(other,axis,edited[axis],a[axis],true);
      require(std::fabs(wrap(relative[axis]-other[axis])-sign*PI/180.)<1.e-12,
              "multi-selection snapped to anchor instead of delta");
    }
    // F32 member round trip includes multiple quaternion products. Simulate
    // those product errors near vertical, independently of the authored cast.
    auto multiply=[](Quaternion a, Quaternion b) {
      Quaternion q {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
                    a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
                    a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
                    a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
      for(auto& v:q) v=static_cast<float>(v); return q;
    };
    for(double sign : {-1.,1.}) for(int step=-100;step<=100;++step) {
      Angles a {.52,sign*(PI/2+step*1.e-6),-.41};
      auto frame=compose({1.2,.8,-.9}); for(auto& v:frame) v=static_cast<float>(v);
      auto q=compose(a); for(auto& v:q) v=static_cast<float>(v);
      auto world=multiply(frame,q); auto inverse=frame;
      for(int i=0;i<3;++i) inverse[i]=-inverse[i];
      auto local=multiply(inverse,world);
      auto read=decompose(local,a);
      for(int axis=0;axis<3;++axis)
        require(std::fabs(wrap(read[axis]-a[axis]))<1.e-10,"member gimbal reference unstable");
    }
    const Angles selected {.4, 2., -.7};
    const auto saved=decompose(compose(selected),selected);
    (void)decompose(compose({-.9,.3,.2}));
    const auto restored=decompose(compose(selected),saved);
    for(int i=0;i<3;++i) require(std::fabs(restored[i]-selected[i])<1.e-12,
                                "reselected orientation branch changed");
    for(double pitch : {-2.,-PI/2,PI/2,2.}) {
      Angles reference {.4,pitch,-.7};
      for(int axis=0;axis<3;++axis) {
        auto edited=reference; edited[axis]+=.01;
        const auto read=decompose(compose(edited),edited);
        for(int i=0;i<3;++i) require(std::fabs(wrap(edited[i]-read[i]))<1.e-12,
                                   "axis edit changed another component");
      }
    }
    std::cout << "PASS: axes/order, legacy yaw, 450 gimbal/boundary cases, 10000 random round trips, 4002 scroll steps, singular read-back, branch retention, independent axis edits, heading preservation, wrap, relative multi deltas, 402 F32 member gimbal cases\n";
    return 0;
 } catch(const std::exception& e) {std::cerr << "FAIL: " << e.what() << "\n"; return 1;}
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',default='cl.exe')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='clone_rotation_') as scratch:
        source=Path(scratch)/'test.cpp'
        source.write_text(TEST,encoding='utf-8')
        exe=Path(scratch)/'test.exe'
        subprocess.run([args.compiler,'/nologo','/std:c++17','/EHsc','/W4','/WX',
                        '/I'+str(ROOT/'indra/newview'),str(source),'/Fe:'+str(exe),
                        '/Fo:'+str(Path(scratch)/'test.obj')],check=True,cwd=scratch)
        return subprocess.run([str(exe)],check=False).returncode
if __name__=='__main__':
    raise SystemExit(main())
