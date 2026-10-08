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
        foreach(_f poppler-qt6 poppler qt@6 qt)
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

# ---- Qt ----
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Xml Concurrent LinguistTools)
set(QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::Xml Qt6::Concurrent)
message(STATUS "Using Qt6 ${Qt6_VERSION}")

# ---- Poppler Qt6 bindings ----
find_package(PkgConfig QUIET)

find_package(Poppler QUIET COMPONENTS Qt6)
if(Poppler_FOUND AND TARGET Poppler::Qt6)
    set(POPPLER_TARGET Poppler::Qt6)
    message(STATUS "Poppler Qt6 found via CMake config")
endif()

if(NOT POPPLER_TARGET AND PKG_CONFIG_FOUND)
    pkg_check_modules(POPPLER_QT poppler-qt6)
    if(POPPLER_QT_FOUND)
        add_library(poppler_qt_pkgconfig INTERFACE IMPORTED)
        target_include_directories(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_INCLUDE_DIRS})
        target_link_directories(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_LIBRARY_DIRS})
        target_link_libraries(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_LIBRARIES})
        target_compile_options(poppler_qt_pkgconfig INTERFACE ${POPPLER_QT_CFLAGS_OTHER})
        set(POPPLER_TARGET poppler_qt_pkgconfig)
        message(STATUS "Poppler Qt6 found via pkg-config ${POPPLER_QT_VERSION}")
    endif()
endif()

if(NOT POPPLER_TARGET)
    # keg-only な Homebrew formula は <prefix>/lib 直下に無いので opt/<formula> も見る
    set(_POPPLER_HINTS ${CMAKE_PREFIX_PATH} /opt/homebrew /usr/local /usr)
    if(HOMEBREW_PREFIX)
        list(APPEND _POPPLER_HINTS
            "${HOMEBREW_PREFIX}/opt/poppler-qt6"
            "${HOMEBREW_PREFIX}/opt/poppler")
    endif()
    find_library(POPPLER_QT_LIB NAMES poppler-qt6
        HINTS ${_POPPLER_HINTS}
        PATH_SUFFIXES lib lib64)
    find_path(POPPLER_QT_INCLUDE NAMES poppler-qt6.h
        HINTS ${_POPPLER_HINTS}
        PATH_SUFFIXES include include/poppler/qt6 include/poppler)
    if(POPPLER_QT_LIB AND POPPLER_QT_INCLUDE)
        add_library(poppler_qt_manual INTERFACE IMPORTED)
        target_include_directories(poppler_qt_manual INTERFACE ${POPPLER_QT_INCLUDE})
        target_link_libraries(poppler_qt_manual INTERFACE ${POPPLER_QT_LIB})
        set(POPPLER_TARGET poppler_qt_manual)
        message(STATUS "Poppler Qt6 found via find_library: ${POPPLER_QT_LIB}")
    else()
        message(FATAL_ERROR "Poppler Qt6 bindings not found.")
    endif()
endif()


# pkg-config からバージョンを取得して POPPLER_VERSION マクロとして渡す
# (poppler-version.h は poppler-global.h を必要とするためインクルード不可)
# Version は .pc の「変数」ではなくフィールドなので pkg_get_variable では取れない。
# pkg_check_modules が設定する <PREFIX>_VERSION を使う。
if(POPPLER_QT_VERSION)
    set(POPPLER_VERSION_STR "${POPPLER_QT_VERSION}")
elseif(PKG_CONFIG_FOUND)
    pkg_check_modules(_POPPLER_VER QUIET poppler-qt6)
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
qt6_add_translation(QM_FILES ${TS_FILES})

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
    COMMAND Qt6::rcc
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
    renderer.cpp
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
    renderer.h
    saveform.hpp
    sequence_matcher.hpp
    startupparameters.h
    status.h
    textitem.hpp
    utils.h
)

# ---- The program ----
add_executable(disparPDF
    main.cpp
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)
target_compile_definitions(disparPDF PRIVATE POPPLER_VERSION="${POPPLER_VERSION_STR}"
    DISPARPDF_VERSION="${PROJECT_VERSION}")
target_link_libraries(disparPDF PRIVATE ${QT_LIBS} ${POPPLER_TARGET})

if(APPLE)
    set_target_properties(disparPDF PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# ---- disparPDFc ----
# Run by the name disparPDFc, disparPDF is always in batch mode (see
# main.cpp).  So disparPDFc is a symbolic link to it or, on Windows, a
# copy; this makes one in the build directory, for the tests, whose path
# is DISPARPDFC.
if(WIN32)
    set(DISPARPDFC ${CMAKE_CURRENT_BINARY_DIR}/disparPDFc.exe)
    add_custom_command(TARGET disparPDF POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:disparPDF> ${DISPARPDFC}
        VERBATIM)
else()
    set(DISPARPDFC ${CMAKE_CURRENT_BINARY_DIR}/disparPDFc)
    add_custom_command(TARGET disparPDF POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                $<TARGET_FILE:disparPDF> ${DISPARPDFC}
        VERBATIM)
endif()

# ---- Tests (see test/CMakeLists.txt); they run the program, so are not
# built when cross compiling ----
option(BUILD_TESTING "Build the tests" ON)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(test)
endif()

# ---- Install ----
include(GNUInstallDirs)
install(TARGETS disparPDF
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)
# disparPDFc: a copy on Windows, otherwise a relative symbolic link, to
# the program in the bundle on macOS
if(WIN32)
    install(PROGRAMS $<TARGET_FILE:disparPDF>
            DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME disparPDFc.exe)
else()
    if(APPLE)
        set(_target ../disparPDF.app/Contents/MacOS/disparPDF)
    else()
        set(_target disparPDF)
    endif()
    install(CODE "
        set(_dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
        message(STATUS \"Installing: \${_dir}/disparPDFc -> ${_target}\")
        file(MAKE_DIRECTORY \"\${_dir}\")
        file(CREATE_LINK ${_target} \"\${_dir}/disparPDFc\" SYMBOLIC)
    ")
endif()
install(FILES README.md CHANGES DESTINATION ${CMAKE_INSTALL_DOCDIR})

# The desktop file and icon, for the menus of Linux and BSD desktops
if(UNIX AND NOT APPLE)
    install(FILES disparPDF.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES images/icon.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps
            RENAME disparPDF.png)
    install(FILES images/icon.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME disparPDF.svg)
endif()

# Shell completions (for both programs)
set(BASH_COMPLETION_DIR ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
    CACHE PATH "Where to install the bash completion")
set(ZSH_COMPLETION_DIR ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
    CACHE PATH "Where to install the zsh completion")
# bash-completion loads the completion for a command from the file of the
# same name
foreach(_prog disparPDF disparPDFc)
    install(FILES completions/disparPDF.bash DESTINATION ${BASH_COMPLETION_DIR}
            RENAME ${_prog})
endforeach()
install(FILES completions/_disparPDF DESTINATION ${ZSH_COMPLETION_DIR})

# The manual page, which covers disparPDFc too, is generated by help2man
# from "disparPDF --help" and "disparPDF --version" (which need no
# display) plus doc_man/disparPDF.h2m.  That runs the program, so when
# cross compiling, or without help2man, the copy of the generated page
# kept in doc_man is installed instead.  After changing the options or
# the .h2m file, refresh that copy with
# "cmake --build build --target update-manpage" and commit it.
# doc_man/disparPDFc.1 only refers to disparPDF.1 (".so").
find_program(HELP2MAN_EXECUTABLE help2man)
set(_cached ${CMAKE_CURRENT_SOURCE_DIR}/doc_man/disparPDF.1)
if(HELP2MAN_EXECUTABLE AND NOT CMAKE_CROSSCOMPILING)
    set(MANPAGE ${CMAKE_CURRENT_BINARY_DIR}/disparPDF.1)
    set(_h2m ${CMAKE_CURRENT_SOURCE_DIR}/doc_man/disparPDF.h2m)
    add_custom_command(
        OUTPUT ${MANPAGE}
        COMMAND ${HELP2MAN_EXECUTABLE} --no-info --include=${_h2m}
                --output=${MANPAGE} $<TARGET_FILE:disparPDF>
        DEPENDS disparPDF ${_h2m}
        COMMENT "Generating the manual page with help2man"
        VERBATIM)
    add_custom_target(manpage ALL DEPENDS ${MANPAGE})
    add_custom_target(update-manpage
        COMMAND ${CMAKE_COMMAND} -E copy ${MANPAGE} ${_cached}
        DEPENDS ${MANPAGE}
        COMMENT "Copying the generated manual page into doc_man"
        VERBATIM)
else()
    set(MANPAGE ${_cached})
    file(STRINGS ${MANPAGE} _th REGEX "^\\.TH " LIMIT_COUNT 1)
    if(NOT _th MATCHES "\"disparPDF ${PROJECT_VERSION}\"")
        message(WARNING "The cached manual page doc_man/disparPDF.1 is not "
            "for version ${PROJECT_VERSION}; refresh it with the "
            "update-manpage target in a native build with help2man.")
    endif()
endif()
install(FILES ${MANPAGE} ${CMAKE_CURRENT_SOURCE_DIR}/doc_man/disparPDFc.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
