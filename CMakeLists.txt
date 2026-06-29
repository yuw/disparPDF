cmake_minimum_required(VERSION 3.16)
project(disparPDF VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC OFF)

# ---- Qt (prefer Qt6, fall back to Qt5) ----
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets PrintSupport Xml LinguistTools)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::Xml)
    message(STATUS "Using Qt6 ${Qt6_VERSION}")
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Xml LinguistTools)
    set(QT_VERSION_MAJOR 5)
    set(QT_LIBS Qt5::Core Qt5::Gui Qt5::Widgets Qt5::PrintSupport Qt5::Xml)
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
    find_library(POPPLER_QT_LIB NAMES ${_POPPLER_LIBNAME}
        HINTS ${CMAKE_PREFIX_PATH} /opt/homebrew /usr/local /usr
        PATH_SUFFIXES lib lib64)
    find_path(POPPLER_QT_INCLUDE NAMES ${_POPPLER_HEADER}
        HINTS ${CMAKE_PREFIX_PATH} /opt/homebrew /usr/local /usr
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
if(PKG_CONFIG_FOUND)
    if(QT_VERSION_MAJOR EQUAL 6)
        pkg_get_variable(POPPLER_VERSION_STR poppler-qt6 Version)
    else()
        pkg_get_variable(POPPLER_VERSION_STR poppler-qt5 Version)
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

install(TARGETS disparPDF disparPDFc
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
