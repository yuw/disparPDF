cmake_minimum_required(VERSION 3.16)
project(disparPDF VERSION 1.0.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC OFF)

# ---- Homebrew (macOS): keg-only な依存を自動で探索パスへ追加 ----
# qt@6 / poppler-qt6 などは keg-only なので、既定の CMAKE_PREFIX_PATH や
# pkg-config の既定パスからは見つからない。ここで自動的に前置しておくことで、
# 環境変数を設定しなくても `cmake -B build` だけでビルドできるようにする。
# また Intel 版 Homebrew (/usr/local) の古い .pc が残っている環境では、
# アーキテクチャ違いのライブラリを掴んでしまうため、必ず先頭に追加する。
function(disparpdf_prepend_brew_prefix _formula)
    if(NOT HOMEBREW_PREFIX)
        return()
    endif()
    set(_opt "${HOMEBREW_PREFIX}/opt/${_formula}")
    if(NOT IS_DIRECTORY "${_opt}")
        return()
    endif()
    # CMAKE_PREFIX_PATH には後ろから追加する（ユーザー指定を優先させるため）
    set(_paths ${CMAKE_PREFIX_PATH})
    list(APPEND _paths "${_opt}")
    set(CMAKE_PREFIX_PATH "${_paths}" PARENT_SCOPE)
    # PKG_CONFIG_PATH は先頭に置く（/usr/local の古い .pc に勝たせるため）
    if(IS_DIRECTORY "${_opt}/lib/pkgconfig")
        set(ENV{PKG_CONFIG_PATH} "${_opt}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
endfunction()

if(APPLE)
    find_program(BREW_EXECUTABLE brew)
    if(BREW_EXECUTABLE)
        execute_process(COMMAND "${BREW_EXECUTABLE}" --prefix
            OUTPUT_VARIABLE HOMEBREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()
    if(HOMEBREW_PREFIX)
        message(STATUS "Homebrew prefix: ${HOMEBREW_PREFIX}")
        # 各 formula は互いに異なる .pc を提供するため相互の順序は問わない。
        # 重要なのは、いずれもが pkg-config の既定パスより前に来ること。
        # qt@5 / poppler-qt5 は Qt6 が無い環境向けの保険。
        foreach(_f poppler-qt6 poppler qt@6 qt qt@5 poppler-qt5)
            disparpdf_prepend_brew_prefix(${_f})
        endforeach()
        # PATH 上の pkg-config が Intel 版 Homebrew (/usr/local) のものだと、
        # 探索パスが x86_64 側に固定され依存解決に失敗する。Homebrew の
        # pkgconf があればそちらを優先する。
        if(NOT PKG_CONFIG_EXECUTABLE AND EXISTS "${HOMEBREW_PREFIX}/bin/pkg-config")
            set(PKG_CONFIG_EXECUTABLE "${HOMEBREW_PREFIX}/bin/pkg-config"
                CACHE FILEPATH "pkg-config executable")
        endif()
    endif()
endif()

# ---- Qt (prefer Qt6, fall back to Qt5) ----
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets PrintSupport Xml Concurrent LinguistTools)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::Xml Qt6::Concurrent)
    message(STATUS "Using Qt6 ${Qt6_VERSION}")
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Xml Concurrent LinguistTools)
    set(QT_VERSION_MAJOR 5)
    set(QT_LIBS Qt5::Core Qt5::Gui Qt5::Widgets Qt5::PrintSupport Qt5::Xml Qt5::Concurrent)
    message(STATUS "Using Qt5 ${Qt5_VERSION}")
endif()

# ---- Poppler Qt bindings ----
find_package(PkgConfig QUIET)

find_package(Poppler QUIET COMPONENTS Qt6)
if(Poppler_FOUND AND TARGET Poppler::Qt6)
    set(POPPLER_TARGET Poppler::Qt6)
    message(STATUS "Poppler Qt6 found via CMake config")
endif()

if(NOT POPPLER_TARGET AND QT_VERSION_MAJOR EQUAL 5)
    find_package(Poppler QUIET COMPONENTS Qt5)
    if(Poppler_FOUND AND TARGET Poppler::Qt5)
        set(POPPLER_TARGET Poppler::Qt5)
        message(STATUS "Poppler Qt5 found via CMake config")
    endif()
endif()

if(NOT POPPLER_TARGET AND PKG_CONFIG_FOUND)
    if(QT_VERSION_MAJOR EQUAL 6)
        pkg_check_modules(POPPLER_QT poppler-qt6)
    else()
        pkg_check_modules(POPPLER_QT poppler-qt5)
    endif()
    if(POPPLER_QT_FOUND)
        add_library(poppler_qt_pkgconfig INTERFACE IMPORTED)
        target_include_directories(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_INCLUDE_DIRS})
        target_link_directories(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_LIBRARY_DIRS})
        target_link_libraries(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_LIBRARIES})
        target_compile_options(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_CFLAGS_OTHER})
        set(POPPLER_TARGET poppler_qt_pkgconfig)
        message(STATUS "Poppler Qt${QT_VERSION_MAJOR} found via pkg-config ${POPPLER_QT_VERSION}")
    endif()
endif()

if(NOT POPPLER_TARGET)
    if(QT_VERSION_MAJOR EQUAL 6)
        set(_POPPLER_LIBNAME poppler-qt6)
        set(_POPPLER_HEADER  poppler-qt6.h)
    else()
        set(_POPPLER_LIBNAME poppler-qt5)
        set(_POPPLER_HEADER  poppler-qt5.h)
    endif()
    # keg-only な Homebrew formula は <prefix>/lib 直下に無いので opt/<formula> も見る
    set(_POPPLER_HINTS ${CMAKE_PREFIX_PATH} /opt/homebrew /usr/local /usr)
    if(HOMEBREW_PREFIX)
        list(APPEND _POPPLER_HINTS
            "${HOMEBREW_PREFIX}/opt/${_POPPLER_LIBNAME}"
            "${HOMEBREW_PREFIX}/opt/poppler")
    endif()
    find_library(POPPLER_QT_LIB NAMES ${_POPPLER_LIBNAME}
        HINTS ${_POPPLER_HINTS}
        PATH_SUFFIXES lib lib64)
    find_path(POPPLER_QT_INCLUDE NAMES ${_POPPLER_HEADER}
        HINTS ${_POPPLER_HINTS}
        PATH_SUFFIXES include include/poppler/qt6 include/poppler/qt5 include/poppler)
    if(POPPLER_QT_LIB AND POPPLER_QT_INCLUDE)
        add_library(poppler_qt_manual INTERFACE IMPORTED)
        target_include_directories(poppler_qt_manual INTERFACE ${POPPLER_QT_INCLUDE})
        target_link_libraries(poppler_qt_manual INTERFACE ${POPPLER_QT_LIB})
        set(POPPLER_TARGET poppler_qt_manual)
        message(STATUS "Poppler Qt${QT_VERSION_MAJOR} found via find_library: ${POPPLER_QT_LIB}")
    else()
        message(FATAL_ERROR "Poppler Qt${QT_VERSION_MAJOR} bindings not found.")
    endif()
endif()


# pkg-config からバージョンを取得して POPPLER_VERSION マクロとして渡す
# (poppler-version.h は poppler-global.h を必要とするためインクルード不可)
# Version は .pc の「変数」ではなくフィールドなので pkg_get_variable では取れない。
# pkg_check_modules が設定する <PREFIX>_VERSION を使う。
if(POPPLER_QT_VERSION)
    set(POPPLER_VERSION_STR "${POPPLER_QT_VERSION}")
elseif(PKG_CONFIG_FOUND)
    pkg_check_modules(_POPPLER_VER QUIET poppler-qt${QT_VERSION_MAJOR})
    if(_POPPLER_VER_VERSION)
        set(POPPLER_VERSION_STR "${_POPPLER_VER_VERSION}")
    endif()
endif()
if(NOT POPPLER_VERSION_STR)
    set(POPPLER_VERSION_STR "unknown")
endif()
message(STATUS "Poppler version: ${POPPLER_VERSION_STR}")

# ---- Translations (.ts → .qm) ----
set(TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/disparPDF_cz.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/disparPDF_de.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/disparPDF_es.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/disparPDF_fr.ts
)
if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_translation(QM_FILES ${TS_FILES})
else()
    qt5_add_translation(QM_FILES ${TS_FILES})
endif()

# resources.qrc がソースディレクトリ相対で .qm を参照しているため、
# ビルド後に .qm をソースディレクトリへコピーするステップを挟む
set(QM_COPIES)
foreach(_qm ${QM_FILES})
    get_filename_component(_name "${_qm}" NAME)
    set(_dst "${CMAKE_CURRENT_SOURCE_DIR}/${_name}")
    add_custom_command(
        OUTPUT  "${_dst}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qm}" "${_dst}"
        DEPENDS "${_qm}"
        VERBATIM
    )
    list(APPEND QM_COPIES "${_dst}")
endforeach()

# ---- Resources (.qrc → .cpp, AUTORCC=OFF なので手動) ----
set(QRC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc")
set(QRC_CPP "${CMAKE_CURRENT_BINARY_DIR}/qrc_resources.cpp")

# GENERATED をマークすることで AUTOMOC がビルド前チェックをスキップする
set_source_files_properties("${QRC_CPP}" PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT  "${QRC_CPP}"
    COMMAND Qt${QT_VERSION_MAJOR}::rcc
            --name resources
            --output "${QRC_CPP}"
            "${QRC_SRC}"
    DEPENDS "${QRC_SRC}" ${QM_COPIES}
    COMMENT "rcc: compiling resources.qrc"
    VERBATIM
)

# ---- Sources ----
set(COMMON_SOURCES
    aboutform.cpp
    batchcompare.cpp
    commandlinemanager.cpp
    compareresults.cpp
    docinfo.cpp
    generic.cpp
    helpform.cpp
    label.cpp
    lineedit.cpp
    mainwindow.cpp
    pagecompare.cpp
    optionsform.cpp
    saveform.cpp
    sequence_matcher.cpp
    startupparameters.cpp
    status.cpp
    textitem.cpp
    utils.cpp
    "${QRC_CPP}"
)

set(COMMON_HEADERS
    aboutform.hpp
    batchcompare.h
    commandlinemanager.h
    compareresults.h
    docinfo.h
    generic.hpp
    helpform.hpp
    label.hpp
    lineedit.hpp
    mainwindow.hpp
    optionsform.hpp
    pagecompare.h
    saveform.hpp
    sequence_matcher.hpp
    startupparameters.h
    status.h
    textitem.hpp
    utils.h
)

# ---- GUI executable ----
add_executable(disparPDF
    main.cpp
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)
target_compile_definitions(disparPDF PRIVATE USE_QT${QT_VERSION_MAJOR} POPPLER_VERSION="${POPPLER_VERSION_STR}")
target_link_libraries(disparPDF PRIVATE ${QT_LIBS} ${POPPLER_TARGET})

# ---- Console-only executable ----
add_executable(disparPDFc
    main.cpp
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)
target_compile_definitions(disparPDFc PRIVATE USE_QT${QT_VERSION_MAJOR} COMPARA_IS_CONSOLE POPPLER_VERSION="${POPPLER_VERSION_STR}")
target_link_libraries(disparPDFc PRIVATE ${QT_LIBS} ${POPPLER_TARGET})

if(APPLE)
    set_target_properties(disparPDF PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# ---- Tests (see test/CMakeLists.txt); they run disparPDFc, so are not
# built when cross compiling ----
option(BUILD_TESTING "Build the tests" ON)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(test)
endif()

install(TARGETS disparPDF disparPDFc
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
