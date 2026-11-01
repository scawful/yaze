# CPack pre-build script (CPACK_PRE_BUILD_SCRIPTS). Runs on the staged DMG
# contents after install() and before the image is built, so the dylib
# copies and the signature it makes are what ships.

file(GLOB _yaze_app_bundles LIST_DIRECTORIES true
    "${CPACK_TEMPORARY_DIRECTORY}/yaze.app"
    "${CPACK_TEMPORARY_DIRECTORY}/*/yaze.app")
list(LENGTH _yaze_app_bundles _yaze_app_count)
if(NOT _yaze_app_count EQUAL 1)
    message(FATAL_ERROR
        "Expected one staged yaze.app under ${CPACK_TEMPORARY_DIRECTORY}, "
        "found: ${_yaze_app_bundles}")
endif()

get_filename_component(_yaze_package_root "${_yaze_app_bundles}" DIRECTORY)
execute_process(
    COMMAND "${CPACK_YAZE_BUNDLE_DYLIBS_SCRIPT}" "${_yaze_package_root}"
    RESULT_VARIABLE _yaze_bundle_result)
if(NOT _yaze_bundle_result EQUAL 0)
    message(FATAL_ERROR "bundle-macos-dylibs.sh failed (${_yaze_bundle_result})")
endif()
