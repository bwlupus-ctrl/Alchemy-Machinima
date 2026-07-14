# sl_reshade_bridge -- ReShade 6.x add-on for the phoenix-reshade-XL viewer.
# Standalone by design: NOT part of the autobuild/viewer solution, so the
# add-on can be iterated without relinking the viewer.
#
#   cmake -S reshade-addon -B reshade-addon/build -G "Visual Studio 18 2026" -A x64
#   cmake --build reshade-addon/build --config Release
#
# Output: sl_reshade_bridge.addon  (copy next to ReShade's opengl32.dll or into
# its configured add-on search path).

cmake_minimum_required(VERSION 3.21)
project(sl_reshade_bridge CXX)

add_library(sl_reshade_bridge SHARED src/sl_reshade_bridge.cpp)

target_include_directories(sl_reshade_bridge PRIVATE external/reshade)
target_compile_features(sl_reshade_bridge PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(sl_reshade_bridge PRIVATE /W4)
endif()

set_target_properties(sl_reshade_bridge PROPERTIES
    OUTPUT_NAME "sl_reshade_bridge"
    SUFFIX ".addon")
