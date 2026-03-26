# ============================================================================
# ФАЙЛ CMAKELISTS.TXT - СЦЕНАРИЙ СБОРКИ ПРОЕКТА LAB_1
# ============================================================================
# Назначение файла:
# 1) описать правила сборки приложения на C++/Qt;
# 2) подключить нужные Qt-модули;
# 3) настроить упаковку macOS bundle после сборки;
# 4) сделать проект переносимым между типовыми установками Qt на macOS.
# ============================================================================

# Команда cmake_minimum_required задает минимальную версию CMake,
# ниже которой данный файл конфигурации запускать нельзя.
cmake_minimum_required(VERSION 3.16)

# Команда project объявляет сам проект CMake.
# Здесь задается имя проекта, его версия и языки,
# которые будут использоваться при сборке.
# LANGUAGES CXX означает, что проект собирается как C++-проект.
project(ModelingMethodsLab1 VERSION 0.1.0 LANGUAGES CXX)

# Переменная CMAKE_CXX_STANDARD задает стандарт C++ для всех целей проекта.
# Здесь фиксируем использование C++17.
set(CMAKE_CXX_STANDARD 17)
# Требуем, чтобы указанный стандарт был действительно применен,
# а не понижен до более старого.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Запрещаем компилятору использовать GNU-расширения поверх стандарта,
# чтобы код оставался ближе к чистому C++17.
set(CMAKE_CXX_EXTENSIONS OFF)

# Команда function объявляет пользовательскую функцию CMake.
# Здесь создаем вспомогательную функцию,
# которая будет добавлять в список только существующие пути.
# out_var - имя выходной переменной, в которую вернется результат.
# ARGN - специальный список "всех остальных аргументов функции".
function(_append_existing_prefixes out_var)
    # Читаем текущее содержимое выходной переменной во временный список prefixes.
    # Конструкция ${${out_var}} означает:
    # 1) сначала взять имя переменной из out_var;
    # 2) затем прочитать значение переменной с таким именем.
    set(prefixes ${${out_var}})
    # Команда foreach организует цикл по списку значений.
    # IN LISTS ARGN означает, что мы перебираем элементы списка ARGN.
    foreach(path IN LISTS ARGN)
        # Проверяем два условия:
        # 1) path не пустой;
        # 2) путь path реально существует в файловой системе.
        if(path AND EXISTS "${path}")
            # Если путь валиден, добавляем его в список prefixes.
            list(APPEND prefixes "${path}")
        endif()
    endforeach()
    # Удаляем дубликаты, чтобы один и тот же путь не попал в список несколько раз.
    list(REMOVE_DUPLICATES prefixes)
    # Возвращаем обновленный список вызывающему коду.
    # PARENT_SCOPE означает, что значение устанавливается
    # не внутри функции, а на уровень выше.
    set(${out_var} "${prefixes}" PARENT_SCOPE)
endfunction()

# Проверяем, идет ли сборка на macOS.
# Переменная APPLE автоматически задается CMake на системах Apple.
# Весь блок ниже нужен только для удобного поиска Qt на macOS.
if(APPLE)
    # Создаем пустой список возможных корней установки Qt.
    set(_qt_candidate_prefixes "")

    # Если Qt6_DIR еще не задан пользователем или средой,
    # пробуем определить его по стандартным путям Homebrew.
    if(NOT Qt6_DIR)
        # Проверяем типовой путь Homebrew на Apple Silicon.
        if(EXISTS "/opt/homebrew/opt/qt/lib/cmake/Qt6")
            # Если каталог найден, сохраняем его в Qt6_DIR.
            set(Qt6_DIR "/opt/homebrew/opt/qt/lib/cmake/Qt6")
        # Иначе проверяем типовой путь для Intel/Homebrew.
        elseif(EXISTS "/usr/local/opt/qt/lib/cmake/Qt6")
            # Если найден этот каталог, используем его.
            set(Qt6_DIR "/usr/local/opt/qt/lib/cmake/Qt6")
        endif()
    endif()

    # Поддерживаем альтернативный сценарий:
    # пользователь передал Qt6_DIR через переменную окружения.
    if(DEFINED ENV{Qt6_DIR} AND EXISTS "$ENV{Qt6_DIR}")
        # Из пути до Qt6_DIR поднимаемся на два уровня выше,
        # чтобы получить корневой префикс установки Qt.
        get_filename_component(_qt_prefix "$ENV{Qt6_DIR}/../.." ABSOLUTE)
        # Добавляем найденный префикс в список кандидатов.
        list(APPEND _qt_candidate_prefixes "${_qt_prefix}")
    endif()

    # Команда file(GLOB ...) собирает список директорий по маске.
    # Здесь ищем версии Qt, установленные через официальный Qt Installer.
    # LIST_DIRECTORIES true говорит CMake возвращать именно каталоги.
    file(GLOB _qt_installer_prefixes LIST_DIRECTORIES true
        "/Applications/Qt/*/macos"
        "$ENV{HOME}/Qt/*/macos"
    )

    # Вызываем нашу вспомогательную функцию и добавляем в список:
    # - каталоги, найденные через Qt Installer;
    # - типовые префиксы Homebrew для разных Qt-пакетов.
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

    # Если хотя бы один префикс найден, подключаем их к CMAKE_PREFIX_PATH.
    # Именно этот список использует find_package при поиске пакетов.
    if(_qt_candidate_prefixes)
        # PREPEND добавляет пути в начало списка,
        # чтобы наши явные варианты имели приоритет.
        list(PREPEND CMAKE_PREFIX_PATH ${_qt_candidate_prefixes})
        # Снова убираем возможные повторы путей.
        list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    endif()

    # Создаем отдельный список дополнительных корней Qt-пакетов.
    # Он пригодится для зависимостей, которые могут лежать отдельно.
    set(_qt_additional_package_prefixes "")
    # Аналогично собираем только реально существующие директории.
    _append_existing_prefixes(
        _qt_additional_package_prefixes
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtcharts"
        "/usr/local/opt/qt"
        "/usr/local/opt/qtsvg"
        "/usr/local/opt/qtcharts"
    )

    # Если список не пустой, превращаем его в строку с разделителем ';'.
    # Именно такой формат ожидает CMake для путевых списков.
    if(_qt_additional_package_prefixes)
        list(JOIN _qt_additional_package_prefixes ";" QT_ADDITIONAL_PACKAGES_PREFIX_PATH)
        # Сохраняем результат в cache-переменную.
        # CACHE делает переменную видимой между повторными конфигурациями.
        # STRING задает тип cache-переменной.
        # FORCE заставляет обновить значение даже если оно уже существовало.
        set(QT_ADDITIONAL_PACKAGES_PREFIX_PATH
            "${QT_ADDITIONAL_PACKAGES_PREFIX_PATH}"
            CACHE STRING "Additional Qt package roots" FORCE)
    endif()
endif()

# Подключаем используемые Qt-модули.
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Concurrent)

# Объявляем исполняемый файл приложения и перечисляем исходники проекта.
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

# Команда set_source_files_properties позволяет назначить свойства
# конкретным исходным файлам.
# Здесь мы говорим CMake, что файл qt.conf должен попасть
# в каталог Resources внутри macOS-приложения .app.
set_source_files_properties(
    # Указываем сам файл, для которого меняем свойства.
    resources/qt.conf
    # Ключевое слово PROPERTIES означает начало списка "свойство = значение".
    PROPERTIES MACOSX_PACKAGE_LOCATION Resources
)

# Команда target_include_directories добавляет каталоги,
# в которых компилятор будет искать заголовочные файлы.
# PRIVATE означает, что этот путь нужен только самой цели ModelingMethodsLab1
# и не должен автоматически передаваться дальше другим целям.
target_include_directories(ModelingMethodsLab1 PRIVATE include)

# Команда set_target_properties задает набор свойств для конкретной цели.
# Здесь настраиваем особенности Qt и параметры macOS bundle.
set_target_properties(
    # Имя цели, к которой относятся свойства.
    ModelingMethodsLab1
    # Ключевое слово PROPERTIES начинает перечисление свойств цели.
    PROPERTIES
        # AUTOMOC включает автоматический запуск Meta-Object Compiler,
        # который нужен для классов Qt с Q_OBJECT.
        AUTOMOC ON
        # AUTORCC включает автоматическую обработку .qrc-файлов ресурсов Qt.
        AUTORCC ON
        # MACOSX_BUNDLE_BUNDLE_NAME задает отображаемое имя .app bundle.
        MACOSX_BUNDLE_BUNDLE_NAME "ModelingMethodsLab1"
        # MACOSX_BUNDLE_GUI_IDENTIFIER задает bundle identifier приложения.
        # Это macOS-идентификатор в стиле обратного домена.
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.modelingmethods.lab1"
)

# Команда target_link_libraries связывает цель
# с нужными библиотеками на этапе линковки.
# PRIVATE снова означает, что эти зависимости нужны только этой цели.
target_link_libraries(ModelingMethodsLab1 PRIVATE Qt6::Widgets Qt6::Charts Qt6::Concurrent)

# Ниже идет отдельный macOS-специфичный блок.
# На macOS мало просто собрать бинарник:
# нужно еще правильно упаковать framework и плагины Qt внутрь .app.
if(APPLE)
    # Команда find_program ищет внешнюю утилиту в системе.
    # Здесь ищем macdeployqt - стандартный инструмент Qt
    # для упаковки macOS bundle.
    find_program(
        # Имя переменной, в которую будет записан путь к найденной программе.
        MACDEPLOYQT_EXECUTABLE
        # NAMES перечисляет допустимые имена исполняемого файла.
        NAMES macdeployqt
        # HINTS задает каталоги, где особенно стоит поискать программу.
        HINTS
            "/opt/homebrew/opt/qt/bin"
            "/usr/local/opt/qt/bin"
    )
    # Ищем системную утилиту codesign для подписи .app bundle.
    find_program(CODESIGN_EXECUTABLE NAMES codesign)
    # Ищем утилиту install_name_tool для переписывания зависимостей dylib/framework.
    find_program(INSTALL_NAME_TOOL_EXECUTABLE NAMES install_name_tool)

    # Создаем переменную, где позже будет храниться путь
    # к платформенному плагину qcocoa.
    set(QT_COCOA_PLUGIN_PATH "")
    # Проверяем типовой путь Homebrew для Apple Silicon.
    if(EXISTS "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
        # Если файл найден, сохраняем его путь.
        set(QT_COCOA_PLUGIN_PATH "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
    # Иначе пробуем типовой путь Homebrew/Intel.
    elseif(EXISTS "/usr/local/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
        # Если найден этот вариант, используем его.
        set(QT_COCOA_PLUGIN_PATH "/usr/local/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
    # В качестве запасного варианта проверяем общий системный каталог Qt.
    elseif(EXISTS "/usr/local/share/qt/plugins/platforms/libqcocoa.dylib")
        # Если и он существует, берём этот путь.
        set(QT_COCOA_PLUGIN_PATH "/usr/local/share/qt/plugins/platforms/libqcocoa.dylib")
    endif()

    # Создаем переменную для пути к SVG icon engine plugin.
    set(QTSVG_ICONENGINE_PLUGIN "")
    # Проверяем типовой путь qtsvg через Homebrew на Apple Silicon.
    if(EXISTS "/opt/homebrew/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
        # Если плагин найден, сохраняем его путь.
        set(QTSVG_ICONENGINE_PLUGIN "/opt/homebrew/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
    # Иначе пробуем аналогичный путь для /usr/local.
    elseif(EXISTS "/usr/local/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
        # Если этот файл существует, используем его.
        set(QTSVG_ICONENGINE_PLUGIN "/usr/local/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
    endif()

    # Создаем переменную для пути к QtSvg.framework.
    set(QTSVG_FRAMEWORK_PATH "")
    # Проверяем типовой путь QtSvg.framework на Apple Silicon.
    if(EXISTS "/opt/homebrew/opt/qtsvg/lib/QtSvg.framework")
        # Если framework найден, сохраняем его путь.
        set(QTSVG_FRAMEWORK_PATH "/opt/homebrew/opt/qtsvg/lib/QtSvg.framework")
    # Иначе пробуем тот же framework в /usr/local.
    elseif(EXISTS "/usr/local/opt/qtsvg/lib/QtSvg.framework")
        # Если найден этот вариант, используем его.
        set(QTSVG_FRAMEWORK_PATH "/usr/local/opt/qtsvg/lib/QtSvg.framework")
    endif()

    # Команда add_custom_command(... POST_BUILD) добавляет действия,
    # которые будут выполнены сразу после сборки цели.
    # Здесь перед новым deploy очищаем старые копии QtSvg.framework
    # и каталога iconengines внутри .app bundle.
    add_custom_command(
        # Привязываем команду к цели приложения.
        TARGET ModelingMethodsLab1 POST_BUILD
        # Запускаем внутреннюю утилиту CMake с ключом -E,
        # чтобы удалить старый QtSvg.framework.
        COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework"
        # Аналогично удаляем старый каталог iconengines.
        COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines"
        # COMMENT - текст, который будет показан в логах сборки.
        COMMENT "Removing stale SVG framework and icon engine before Qt deployment"
        # VERBATIM заставляет CMake корректно экранировать аргументы команд.
        VERBATIM
    )

    # Если macdeployqt удалось найти,
    # добавляем шаг автоматической упаковки Qt framework в bundle.
    if(MACDEPLOYQT_EXECUTABLE)
        # get_filename_component(... REALPATH) приводит путь к каноническому виду.
        # Это помогает избежать проблем с симлинками.
        get_filename_component(MACDEPLOYQT_EXECUTABLE_REAL "${MACDEPLOYQT_EXECUTABLE}" REALPATH)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            # Запускаем macdeployqt для собранного .app bundle.
            COMMAND "${MACDEPLOYQT_EXECUTABLE_REAL}"
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>"
                    # -no-plugins запрещает копировать все плагины автоматически.
                    # Нужные плагины мы добавляем вручную ниже.
                    -no-plugins
                    # -always-overwrite разрешает перезаписывать уже существующие файлы.
                    -always-overwrite
            COMMENT "Deploying Qt frameworks and plugins into app bundle"
            VERBATIM
        )
    endif()

    # Если путь к qcocoa найден, копируем этот плагин в bundle.
    if(QT_COCOA_PLUGIN_PATH)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            # Сначала гарантируем существование каталога platforms.
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/platforms"
            # Затем копируем сам плагин, но только если файл отличается.
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QT_COCOA_PLUGIN_PATH}"
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/platforms/libqcocoa.dylib"
            COMMENT "Copying Qt cocoa platform plugin into app bundle"
            VERBATIM
        )
    endif()

    # Этот блок выполняется только если найден SVG icon engine plugin.
    # Он нужен для корректной загрузки SVG-иконок в интерфейсе.
    if(QTSVG_ICONENGINE_PLUGIN)
        # Если найден и сам QtSvg.framework, копируем его в bundle.
        if(QTSVG_FRAMEWORK_PATH)
            add_custom_command(
                TARGET ModelingMethodsLab1 POST_BUILD
                # Перед копированием удаляем старую копию framework,
                # чтобы не смешивать файлы от разных сборок.
                COMMAND "${CMAKE_COMMAND}" -E rm -rf
                        "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework"
                # Команда ditto на macOS удобно копирует каталоги framework целиком.
                # Здесь переносим QtSvg.framework внутрь .app bundle.
                COMMAND /usr/bin/ditto
                        "${QTSVG_FRAMEWORK_PATH}"
                        "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework"
                COMMENT "Copying QtSvg framework into app bundle"
                VERBATIM
            )

            # Если install_name_tool найден,
            # после копирования исправляем внутренние зависимости framework.
            if(INSTALL_NAME_TOOL_EXECUTABLE)
                add_custom_command(
                    TARGET ModelingMethodsLab1 POST_BUILD
                    # Запускаем отдельный CMake-скрипт в script mode.
                    # Параметр -D... передает переменные в этот скрипт.
                    # FRAMEWORK_BINARY - путь к основному бинарнику QtSvg.framework.
                    # INSTALL_NAME_TOOL_EXECUTABLE - путь к утилите install_name_tool.
                    # Ключ -P указывает CMake выполнить заданный .cmake-скрипт.
                    COMMAND "${CMAKE_COMMAND}"
                            -DFRAMEWORK_BINARY="$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/Frameworks/QtSvg.framework/Versions/A/QtSvg"
                            -DINSTALL_NAME_TOOL_EXECUTABLE="${INSTALL_NAME_TOOL_EXECUTABLE}"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixQtSvgFrameworkPaths.cmake"
                    COMMENT "Rewriting QtSvg framework dependencies to bundled frameworks"
                    VERBATIM
                )
            endif()
        endif()

        # Создаем каталог iconengines внутри bundle и копируем туда SVG icon engine.
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            # Гарантируем существование каталога для icon engine plugin.
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines"
            # Копируем сам plugin-файл, если он отличается от уже лежащего в bundle.
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QTSVG_ICONENGINE_PLUGIN}"
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines/libqsvgicon.dylib"
            COMMENT "Copying Qt SVG icon engine plugin into app bundle"
            VERBATIM
        )

        # Если install_name_tool найден,
        # аналогично исправляем зависимости у самого SVG icon engine plugin.
        if(INSTALL_NAME_TOOL_EXECUTABLE)
            add_custom_command(
                TARGET ModelingMethodsLab1 POST_BUILD
                # Передаем в отдельный CMake-скрипт путь к plugin-файлу
                # и путь к install_name_tool.
                COMMAND "${CMAKE_COMMAND}"
                        -DPLUGIN="$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines/libqsvgicon.dylib"
                        -DINSTALL_NAME_TOOL_EXECUTABLE="${INSTALL_NAME_TOOL_EXECUTABLE}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixQtPluginPaths.cmake"
                COMMENT "Rewriting SVG icon engine Qt dependencies to bundled frameworks"
                VERBATIM
            )
        endif()
    endif()

    # Следующий блок удаляет внешние Qt rpath у главного исполняемого файла.
    # Rpath - это встроенный список каталогов, где бинарник ищет библиотеки.
    # Если оставить внешний Qt rpath, приложение может подмешать
    # второй комплект Qt из Homebrew или другой установки.
    if(INSTALL_NAME_TOOL_EXECUTABLE)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            # Запускаем отдельный CMake-скрипт, который чистит внешние rpath.
            # TARGET_FILE - путь к основному исполняемому файлу внутри .app.
            COMMAND "${CMAKE_COMMAND}"
                    -DTARGET_FILE="$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/MacOS/ModelingMethodsLab1"
                    -DINSTALL_NAME_TOOL_EXECUTABLE="${INSTALL_NAME_TOOL_EXECUTABLE}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RemoveExternalQtRpaths.cmake"
            COMMENT "Removing external Qt rpaths from app executable"
            VERBATIM
        )
    endif()

    # После всех модификаций bundle нужно снова подписать.
    # Это важно, потому что install_name_tool и копирование framework/plugin
    # могут сделать старую подпись недействительной.
    if(CODESIGN_EXECUTABLE)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            # Запускаем системную утилиту codesign.
            COMMAND "${CODESIGN_EXECUTABLE}"
                    # --force разрешает перезаписать существующую подпись.
                    --force
                    # --deep подписывает не только сам .app,
                    # но и вложенные framework, dylib и плагины.
                    --deep
                    # --sign - задает сертификат подписи.
                    # Здесь символ "-" означает ad-hoc подпись без реального сертификата.
                    --sign
                    -
                    # Указываем сам .app bundle, который нужно подписать.
                    "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>"
            COMMENT "Applying ad-hoc code signature to app bundle"
            VERBATIM
        )
    endif()
endif()
