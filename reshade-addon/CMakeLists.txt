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

# ReShade add-on API version to REQUEST. Must be <= the version the INSTALLED
# ReShade supports, or register_addon() fails outright and the add-on never loads
# ("Failed to register add-on, because the requested API version (20) is not
# supported (18)!"). external/reshade's headers describe API 20, but the ReShade
# shipped with this viewer is API 18 -- hence the pin (and the historical
# hand-built binary named "sl_reshade_bridge_api18.dll").
# Safe because we only call API-18-era methods; ReShade appends new vtable
# entries, so old indices remain valid. Raise this only after updating ReShade.
set(RESHADE_API_VERSION "18" CACHE STRING
    "ReShade add-on API version to request (must be <= installed ReShade's)")
target_compile_definitions(sl_reshade_bridge PRIVATE
                           RESHADE_API_VERSION=${RESHADE_API_VERSION})

if(MSVC)
    target_compile_options(sl_reshade_bridge PRIVATE /W4)
endif()

# STATIC CRT (/MT). A ReShade add-on is LoadLibrary'd into the viewer process:
# linking the dynamic CRT makes the add-on fail to load outright if
# MSVCP140/VCRUNTIME140 are not resolvable at that moment -- which presents as
# "the bridge silently does nothing". Static keeps it self-contained (and matches
# the hand-built cl.exe binary this project shipped before CMake was used).
# Requires CMP0091=NEW, which is default at cmake_minimum_required(3.21).
set_property(TARGET sl_reshade_bridge PROPERTY
             MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_target_properties(sl_reshade_bridge PROPERTIES
    OUTPUT_NAME "sl_reshade_bridge"
    SUFFIX ".addon")
