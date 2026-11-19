if(NOT DEFINED RUNTIME_SOURCE_DIR OR NOT DEFINED RUNTIME_DEST_DIR OR
   NOT DEFINED RUNTIME_INDEX_FILE OR NOT DEFINED RUNTIME_STAMP_FILE)
  message(FATAL_ERROR
    "stage_runtime_assets.cmake requires source, destination, index, and stamp paths")
endif()

file(REAL_PATH "${RUNTIME_SOURCE_DIR}" runtime_source)
file(MAKE_DIRECTORY "${RUNTIME_DEST_DIR}")
file(REAL_PATH "${RUNTIME_DEST_DIR}" runtime_dest)

# These are the authored runtime assets that must track ordinary developer
# builds. Logging controls are deliberately omitted so a developer's staged
# logging configuration is not replaced on every build.
file(GLOB app_setting_assets LIST_DIRECTORIES FALSE
  "${runtime_source}/app_settings/*.ini"
  "${runtime_source}/app_settings/*.xml")
set(filtered_app_setting_assets)
foreach(app_setting_file IN LISTS app_setting_assets)
  get_filename_component(app_setting_name "${app_setting_file}" NAME)
  string(TOLOWER "${app_setting_name}" app_setting_name_lower)
  if(NOT app_setting_name_lower STREQUAL "logcontrol.xml" AND
     NOT app_setting_name_lower STREQUAL "logcontrol-dev.xml")
    list(APPEND filtered_app_setting_assets "${app_setting_file}")
  endif()
endforeach()
set(app_setting_assets ${filtered_app_setting_assets})

file(GLOB_RECURSE shader_assets LIST_DIRECTORIES FALSE
  "${runtime_source}/app_settings/shaders/*")
file(GLOB_RECURSE dust_assets LIST_DIRECTORIES FALSE
  "${runtime_source}/app_settings/dust/*")
file(GLOB_RECURSE skin_xui_assets LIST_DIRECTORIES FALSE
  "${runtime_source}/skins/*/xui/*.xml")
file(GLOB skin_root_assets LIST_DIRECTORIES FALSE
  "${runtime_source}/skins/*/*.xml"
  "${runtime_source}/skins/*/*.json")

set(runtime_assets
  ${app_setting_assets}
  ${shader_assets}
  ${dust_assets}
  ${skin_xui_assets}
  ${skin_root_assets})
list(REMOVE_DUPLICATES runtime_assets)
list(SORT runtime_assets)

set(runtime_asset_relatives)
set(runtime_asset_signature)
foreach(source_file IN LISTS runtime_assets)
  file(RELATIVE_PATH relative_file "${runtime_source}" "${source_file}")
  list(APPEND runtime_asset_relatives "${relative_file}")
  file(SHA256 "${source_file}" source_hash)
  string(APPEND runtime_asset_signature "${relative_file}:${source_hash}\n")

  set(destination_file "${runtime_dest}/${relative_file}")
  get_filename_component(destination_dir "${destination_file}" DIRECTORY)
  file(MAKE_DIRECTORY "${destination_dir}")
  file(COPY_FILE "${source_file}" "${destination_file}" ONLY_IF_DIFFERENT)
endforeach()

set(runtime_asset_comparison_relatives ${runtime_asset_relatives})
if(WIN32)
  list(TRANSFORM runtime_asset_comparison_relatives TOLOWER)
endif()

# Remove only files that this staging script recorded on a previous run and
# that have since been renamed or deleted in the source tree. The strict path
# allow-list and non-recursive removal keep cleanup inside the managed runtime
# asset families while preserving user/developer files such as logcontrol.xml.
set(asset_index "${RUNTIME_INDEX_FILE}")
if(EXISTS "${asset_index}")
  file(STRINGS "${asset_index}" previous_runtime_assets)
endif()

foreach(previous_file IN LISTS previous_runtime_assets)
    string(REPLACE "\\" "/" normalized_previous_file "${previous_file}")
    set(comparison_previous_file "${normalized_previous_file}")
    string(TOLOWER "${normalized_previous_file}" reserved_previous_file)
    if(WIN32)
      string(TOLOWER "${comparison_previous_file}" comparison_previous_file)
    endif()
    if(comparison_previous_file MATCHES "(^|/)[.][.](/|$)" OR
       comparison_previous_file MATCHES "^[a-z]:/" OR
       comparison_previous_file MATCHES "^/" OR
       reserved_previous_file MATCHES
         "^app_settings/(logcontrol(-dev)?|settings_install)[.]xml$")
      continue()
    endif()

    if(comparison_previous_file MATCHES
        "^(app_settings/[^/]+[.](ini|xml)|app_settings/(shaders|dust)/.+|skins/[^/]+/xui/.+[.]xml|skins/[^/]+/[^/]+[.](xml|json))$")
      list(FIND runtime_asset_comparison_relatives
        "${comparison_previous_file}" current_index)
      if(current_index EQUAL -1)
        set(stale_file "${runtime_dest}/${normalized_previous_file}")
        if(EXISTS "${stale_file}" OR IS_SYMLINK "${stale_file}")
          file(REAL_PATH "${stale_file}" resolved_stale_file)
          file(RELATIVE_PATH resolved_stale_relative
            "${runtime_dest}" "${resolved_stale_file}")
          string(REPLACE "\\" "/" normalized_resolved_stale_relative
            "${resolved_stale_relative}")
          if(NOT IS_ABSOLUTE "${normalized_resolved_stale_relative}" AND
             NOT normalized_resolved_stale_relative MATCHES "^[.][.](/|$)")
            file(REMOVE "${stale_file}")
          endif()
        endif()
      endif()
    endif()
endforeach()

string(REPLACE ";" "\n" asset_index_contents "${runtime_asset_relatives}")
set(new_asset_index "${asset_index_contents}\n")
set(old_asset_index)
if(EXISTS "${asset_index}")
  file(READ "${asset_index}" old_asset_index)
endif()
if(NOT old_asset_index STREQUAL new_asset_index)
  get_filename_component(asset_index_dir "${asset_index}" DIRECTORY)
  file(MAKE_DIRECTORY "${asset_index_dir}")
  file(WRITE "${asset_index}" "${new_asset_index}")
endif()

string(SHA256 runtime_asset_digest "${runtime_asset_signature}")
set(old_asset_digest)
if(EXISTS "${RUNTIME_STAMP_FILE}")
  file(READ "${RUNTIME_STAMP_FILE}" old_asset_digest)
  string(STRIP "${old_asset_digest}" old_asset_digest)
endif()
if(NOT old_asset_digest STREQUAL runtime_asset_digest)
  get_filename_component(runtime_stamp_dir "${RUNTIME_STAMP_FILE}" DIRECTORY)
  file(MAKE_DIRECTORY "${runtime_stamp_dir}")
  file(WRITE "${RUNTIME_STAMP_FILE}" "${runtime_asset_digest}\n")
endif()
