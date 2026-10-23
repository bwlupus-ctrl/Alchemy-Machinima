// Standalone host side of relight_world_gl_check.py. Uses the viewer's aligned GLM ABI
// and the actual production helper, rather than reimplementing its serialization in Python.
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES 1
#define GLM_ENABLE_EXPERIMENTAL 1
#define GLM_FORCE_SSE2 1
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <cstring>
#include "alworldshadowmath.h"
int main(int argc, char**)
{
    static_assert(sizeof(glm::mat3) == 48, "negative control requires viewer aligned GLM");
    if (argc > 1)
    {
        float matrix[4][4]; double lo[3],hi[3],p[3],radius,floor;
        for (auto& row : matrix) for (float& value : row) if (std::scanf("%f",&value)!=1) return 1;
        for (double& value : lo) if (std::scanf("%lf",&value)!=1) return 1;
        for (double& value : hi) if (std::scanf("%lf",&value)!=1) return 1;
        for (double& value : p) if (std::scanf("%lf",&value)!=1) return 1;
        if (std::scanf("%lf %lf",&radius,&floor)!=2) return 1;
        double outLo[3],outHi[3];
        ALWorldShadow::transformedAabb(matrix,lo,hi,outLo,outHi);
        const double reach = ALWorldShadow::chebyshevReach(outLo,outHi,p);
        std::printf("%.12g\n",ALWorldShadow::emitterNearPlane(floor,reach,radius));
        return 0;
    }
    glm::mat4 view(1.f);
    while (true)
    {
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                if (std::scanf("%f", &view[col][row]) != 1) return 0;
        float packed[9], old[9];
        ALWorldShadow::packViewRotation(view, packed);
        // Make the padding deterministic for the negative control. The broken
        // production copy read these padding words as rotation components.
        glm::mat3 aligned;
        std::memset(&aligned,0,sizeof(aligned));
        for (int col=0; col<3; ++col) for (int row=0; row<3; ++row) aligned[col][row] = view[col][row];
        std::memcpy(old, glm::value_ptr(aligned), sizeof(old));
        for (float v : packed) std::printf("%.9g ", v);
        for (float v : old) std::printf("%.9g ", v);
        std::puts("");
        std::fflush(stdout);
    }
}
