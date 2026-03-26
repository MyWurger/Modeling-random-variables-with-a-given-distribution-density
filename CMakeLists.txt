cmake_minimum_required(VERSION 3.16)

project(ModelingMethodsLab1 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(_append_existing_prefixes out_var)
    set(prefixes ${${out_var}})
    foreach(path IN LISTS ARGN)
        if(path AND EXISTS "${path}")
            list(APPEND prefixes "${path}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES prefixes)
    set(${out_var} "${prefixes}" PARENT_SCOPE)
endfunction()

if(APPLE)
    set(_qt_candidate_prefixes "")

    if(NOT Qt6_DIR)
        if(EXISTS "/opt/homebrew/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/opt/homebrew/opt/qt/lib/cmake/Qt6")
        elseif(EXISTS "/usr/local/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/usr/local/opt/qt/lib/cmake/Qt6")
        endif()
    endif()

    if(DEFINED ENV{Qt6_DIR} AND EXISTS "$ENV{Qt6_DIR}")
        get_filename_component(_qt_prefix "$ENV{Qt6_DIR}/../.." ABSOLUTE)
        list(APPEND _qt_candidate_prefixes "${_qt_prefix}")
    endif()

    file(GLOB _qt_installer_prefixes LIST_DIRECTORIES true
        "/Applications/Qt/*/macos"
        "$ENV{HOME}/Qt/*/macos"
    )

    _append_existing_prefixes(
        _qt_candidate_prefixes
        ${_qt_installer_prefixes}
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/qt6"
        "/opt/homebrew/opt/qtbase"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtcharts"
        "/usr/local/opt/qt"
        "/usr/local/opt/qt6"
        "/usr/local/opt/qtbase"
        "/usr/local/opt/qtsvg"
        "/usr/local/opt/qtcharts"
    )

    if(_qt_candidate_prefixes)
        list(PREPEND CMAKE_PREFIX_PATH ${_qt_candidate_prefixes})
        list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    endif()

    set(_qt_additional_package_prefixes "")
    _append_existing_prefixes(
        _qt_additional_package_prefixes
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtcharts"
        "/usr/local/opt/qt"
        "/usr/local/opt/qtsvg"
        "/usr/local/opt/qtcharts"
    )

    if(_qt_additional_package_prefixes)
        list(JOIN _qt_additional_package_prefixes ";" QT_ADDITIONAL_PACKAGES_PREFIX_PATH)
        set(QT_ADDITIONAL_PACKAGES_PREFIX_PATH
            "${QT_ADDITIONAL_PACKAGES_PREFIX_PATH}"
            CACHE STRING "Additional Qt package roots" FORCE)
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Concurrent)

add_executable(
    ModelingMethodsLab1
    MACOSX_BUNDLE
    main.cpp
    include/MainWindow.h
    include/PlotChartWidget.h
    include/SamplingEngine.h
    include/SamplingTypes.h
    include/VariantDistribution.h
    resources/qt.conf
    resources/resources.qrc
    src/MainWindow.cpp
    src/PlotChartWidget.cpp
    src/SamplingEngine.cpp
)

set_source_files_properties(
    resources/qt.conf
    PROPERTIES MACOSX_PACKAGE_LOCATION Resources
)

target_include_directories(ModelingMethodsLab1 PRIVATE include)

set_target_properties(
    ModelingMethodsLab1
    PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        MACOSX_BUNDLE_BUNDLE_NAME "ModelingMethodsLab1"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.modelingmethods.lab1"
)

target_link_libraries(ModelingMethodsLab1 PRIVATE Qt6::Widgets Qt6::Charts Qt6::Concurrent)

if(APPLE)
    find_program(
        MACDEPLOYQT_EXECUTABLE
        NAMES macdeployqt
        HINTS
            "/opt/homebrew/opt/qt/bin"
            "/usr/local/opt/qt/bin"
    )
    find_program(CODESIGN_EXECUTABLE NAMES codesign)
    find_program(INSTALL_NAME_TOOL_EXECUTABLE NAMES install_name_tool)

    set(QT_COCOA_PLUGIN_PATH "")
    if(EXISTS "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
        set(QT_COCOA_PLUGIN_PATH "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
    elseif(EXISTS "/usr/local/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
        set(QT_COCOA_PLUGIN_PATH "/usr/local/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
    elseif(EXISTS "/usr/local/share/qt/plugins/platforms/libqcocoa.dylib")
        set(QT_COCOA_PLUGIN_PATH "/usr/local/share/qt/plugins/platforms/libqcocoa.dylib")
    endif()

    set(QTSVG_ICONENGINE_PLUGIN "")
    if(EXISTS "/opt/homebrew/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
        set(QTSVG_ICONENGINE_PLUGIN "/opt/homebrew/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
    elseif(EXISTS "/usr/local/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
        set(QTSVG_ICONENGINE_PLUGIN "/usr/local/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
    endif()

    set(QTSVG_FRAMEWORK_PATH "")
    if(EXISTS "/opt/homebrew/opt/qtsvg/lib/QtSvg.framework")
        set(QTSVG_FRAMEWORK_PATH "/opt/homebrew/opt/qtsvg/lib/QtSvg.framework")
    elseif(EXISTS "/usr/local/opt/qtsvg/lib/QtSvg.framework")
        set(QTSVG_FRAMEWORK_PATH "/usr/local/opt/qtsvg/lib/QtSvg.framework")
    endif()

    add_custom_command(
        TARGET ModelingMethodsLab1 POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines"
        COMMENT "Removing stale SVG framework and icon engine before Qt deployment"
        VERBATIM
    )

    if(MACDEPLOYQT_EXECUTABLE)
        get_filename_component(MACDEPLOYQT_EXECUTABLE_REAL "${MACDEPLOYQT_EXECUTABLE}" REALPATH)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE_REAL}"
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>"
                    -no-plugins
                    -always-overwrite
            COMMENT "Deploying Qt frameworks and plugins into app bundle"
            VERBATIM
        )
    endif()

    if(QT_COCOA_PLUGIN_PATH)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QT_COCOA_PLUGIN_PATH}"
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/platforms/libqcocoa.dylib"
            COMMENT "Copying Qt cocoa platform plugin into app bundle"
            VERBATIM
        )
    endif()

    if(QTSVG_ICONENGINE_PLUGIN)
        if(QTSVG_FRAMEWORK_PATH)
            add_custom_command(
                TARGET ModelingMethodsLab1 POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E rm -rf
                        "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework"
                COMMAND /usr/bin/ditto
                        "${QTSVG_FRAMEWORK_PATH}"
                        "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework"
                COMMENT "Copying QtSvg framework into app bundle"
                VERBATIM
            )

            if(INSTALL_NAME_TOOL_EXECUTABLE)
                add_custom_command(
                    TARGET ModelingMethodsLab1 POST_BUILD
                    COMMAND "${CMAKE_COMMAND}"
                            -DFRAMEWORK_BINARY="$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework/Versions/A/QtSvg"
                            -DINSTALL_NAME_TOOL_EXECUTABLE="${INSTALL_NAME_TOOL_EXECUTABLE}"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixQtSvgFrameworkPaths.cmake"
                    COMMENT "Rewriting QtSvg framework dependencies to bundled frameworks"
                    VERBATIM
                )
            endif()
        endif()

        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QTSVG_ICONENGINE_PLUGIN}"
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines/libqsvgicon.dylib"
            COMMENT "Copying Qt SVG icon engine plugin into app bundle"
            VERBATIM
        )

        if(INSTALL_NAME_TOOL_EXECUTABLE)
            add_custom_command(
                TARGET ModelingMethodsLab1 POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                        -DPLUGIN="$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines/libqsvgicon.dylib"
                        -DINSTALL_NAME_TOOL_EXECUTABLE="${INSTALL_NAME_TOOL_EXECUTABLE}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixQtPluginPaths.cmake"
                COMMENT "Rewriting SVG icon engine Qt dependencies to bundled frameworks"
                VERBATIM
            )
        endif()
    endif()

    if(INSTALL_NAME_TOOL_EXECUTABLE)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    -DTARGET_FILE="$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/MacOS/ModelingMethodsLab1"
                    -DINSTALL_NAME_TOOL_EXECUTABLE="${INSTALL_NAME_TOOL_EXECUTABLE}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RemoveExternalQtRpaths.cmake"
            COMMENT "Removing external Qt rpaths from app executable"
            VERBATIM
        )
    endif()

    if(CODESIGN_EXECUTABLE)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND "${CODESIGN_EXECUTABLE}"
                    --force
                    --deep
                    --sign
                    -
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>"
            COMMENT "Applying ad-hoc code signature to app bundle"
            VERBATIM
        )
    endif()
endif()
