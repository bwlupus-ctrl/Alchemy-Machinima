# Both independently configured binaries call this one canonical implementation.
function(sl_addon_source_hash root)
    find_program(SL_HASH_PYTHON NAMES python3 python REQUIRED)
    execute_process(COMMAND "${SL_HASH_PYTHON}" "${root}/cmake/SLAddonSourceHash.py"
        "${root}" "${CMAKE_CURRENT_BINARY_DIR}/sl_bridge_build_info.h"
        RESULT_VARIABLE hash_result OUTPUT_VARIABLE hash_manifest ERROR_VARIABLE hash_error)
    if(NOT hash_result EQUAL 0)
        message(FATAL_ERROR "SL add-on manifest failed: ${hash_error}")
    endif()
    message(STATUS "SL add-on SHA-256 and manifest:\n${hash_manifest}")
    file(GLOB_RECURSE hash_sources CONFIGURE_DEPENDS "${root}/reshade-addon/src/*")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${hash_sources}
        "${root}/reshade-addon/CMakeLists.txt" "${root}/indra/newview/llreshadebridgeabi.h"
        "${root}/cmake/SLAddonSourceHash.py")
    include_directories("${CMAKE_CURRENT_BINARY_DIR}")
endfunction()
