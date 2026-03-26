if(NOT DEFINED FRAMEWORK_BINARY OR FRAMEWORK_BINARY STREQUAL "")
    message(FATAL_ERROR "FRAMEWORK_BINARY path is not provided")
endif()

if(NOT DEFINED INSTALL_NAME_TOOL_EXECUTABLE OR INSTALL_NAME_TOOL_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "INSTALL_NAME_TOOL_EXECUTABLE is not provided")
endif()

string(REPLACE "\\ " " " FRAMEWORK_BINARY "${FRAMEWORK_BINARY}")
string(REPLACE "\\ " " " INSTALL_NAME_TOOL_EXECUTABLE "${INSTALL_NAME_TOOL_EXECUTABLE}")
string(REPLACE "\"" "" FRAMEWORK_BINARY "${FRAMEWORK_BINARY}")
string(REPLACE "\"" "" INSTALL_NAME_TOOL_EXECUTABLE "${INSTALL_NAME_TOOL_EXECUTABLE}")

find_program(OTOOL_EXECUTABLE NAMES otool REQUIRED)

execute_process(
    COMMAND "${OTOOL_EXECUTABLE}" -L "${FRAMEWORK_BINARY}"
    OUTPUT_VARIABLE framework_deps
    RESULT_VARIABLE otool_result
)

if(NOT otool_result EQUAL 0)
    message(FATAL_ERROR "Failed to inspect framework dependencies: ${FRAMEWORK_BINARY}")
endif()

set(app_qtsvg "@executable_path/../Frameworks/QtSvg.framework/Versions/A/QtSvg")
set(app_qtgui "@executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui")
set(app_qtcore "@executable_path/../Frameworks/QtCore.framework/Versions/A/QtCore")

execute_process(
    COMMAND "${INSTALL_NAME_TOOL_EXECUTABLE}" -id "${app_qtsvg}" "${FRAMEWORK_BINARY}"
    RESULT_VARIABLE id_result
)

if(NOT id_result EQUAL 0)
    message(FATAL_ERROR "Failed to rewrite install name for ${FRAMEWORK_BINARY}")
endif()

function(_rewrite_if_present old_path new_path)
    if(framework_deps MATCHES "${old_path}")
        execute_process(
            COMMAND "${INSTALL_NAME_TOOL_EXECUTABLE}" -change "${old_path}" "${new_path}" "${FRAMEWORK_BINARY}"
            RESULT_VARIABLE rewrite_result
        )
        if(NOT rewrite_result EQUAL 0)
            message(FATAL_ERROR "Failed to rewrite dependency ${old_path} in ${FRAMEWORK_BINARY}")
        endif()
    endif()
endfunction()

_rewrite_if_present("/opt/homebrew/opt/qtsvg/lib/QtSvg.framework/Versions/A/QtSvg" "${app_qtsvg}")
_rewrite_if_present("/usr/local/opt/qtsvg/lib/QtSvg.framework/Versions/A/QtSvg" "${app_qtsvg}")
_rewrite_if_present("/opt/homebrew/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui" "${app_qtgui}")
_rewrite_if_present("/usr/local/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui" "${app_qtgui}")
_rewrite_if_present("/opt/homebrew/opt/qtbase/lib/QtCore.framework/Versions/A/QtCore" "${app_qtcore}")
_rewrite_if_present("/usr/local/opt/qtbase/lib/QtCore.framework/Versions/A/QtCore" "${app_qtcore}")
