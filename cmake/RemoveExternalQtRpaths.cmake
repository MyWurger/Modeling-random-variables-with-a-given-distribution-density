if(NOT DEFINED TARGET_FILE OR TARGET_FILE STREQUAL "")
    message(FATAL_ERROR "TARGET_FILE path is not provided")
endif()

if(NOT DEFINED INSTALL_NAME_TOOL_EXECUTABLE OR INSTALL_NAME_TOOL_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "INSTALL_NAME_TOOL_EXECUTABLE is not provided")
endif()

string(REPLACE "\\ " " " TARGET_FILE "${TARGET_FILE}")
string(REPLACE "\\ " " " INSTALL_NAME_TOOL_EXECUTABLE "${INSTALL_NAME_TOOL_EXECUTABLE}")
string(REPLACE "\"" "" TARGET_FILE "${TARGET_FILE}")
string(REPLACE "\"" "" INSTALL_NAME_TOOL_EXECUTABLE "${INSTALL_NAME_TOOL_EXECUTABLE}")

find_program(OTOOL_EXECUTABLE NAMES otool REQUIRED)

execute_process(
    COMMAND "${OTOOL_EXECUTABLE}" -l "${TARGET_FILE}"
    OUTPUT_VARIABLE target_load_commands
    RESULT_VARIABLE otool_result
)

if(NOT otool_result EQUAL 0)
    message(FATAL_ERROR "Failed to inspect load commands: ${TARGET_FILE}")
endif()

function(_delete_rpath_if_present old_rpath)
    if(target_load_commands MATCHES "path ${old_rpath}")
        execute_process(
            COMMAND "${INSTALL_NAME_TOOL_EXECUTABLE}" -delete_rpath "${old_rpath}" "${TARGET_FILE}"
            RESULT_VARIABLE rewrite_result
        )
        if(NOT rewrite_result EQUAL 0)
            message(FATAL_ERROR "Failed to delete rpath ${old_rpath} in ${TARGET_FILE}")
        endif()
    endif()
endfunction()

_delete_rpath_if_present("/opt/homebrew/opt/qt/lib")
_delete_rpath_if_present("/usr/local/opt/qt/lib")
_delete_rpath_if_present("/opt/homebrew/opt/qtbase/lib")
_delete_rpath_if_present("/usr/local/opt/qtbase/lib")
