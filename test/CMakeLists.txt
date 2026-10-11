# Tests, run with "ctest" in the build directory.  They need no display.

# Unit tests of the page boundaries (pagebox.cpp), on data/boxes.pdf
add_executable(pageboxtest pageboxtest.cpp ${CMAKE_SOURCE_DIR}/pagebox.cpp)
target_link_libraries(pageboxtest PRIVATE ${QT_LIBS} ${POPPLER_TARGET}
    disparpdf_qpdf)
add_test(NAME pagebox
         COMMAND pageboxtest ${CMAKE_CURRENT_SOURCE_DIR}/data/boxes.pdf)

# Tests of what is drawn for pages compared inside a page box, and of the
# window's page box controls (rendertest.cpp, guitest.cpp).  They are
# built from the program's own sources, compiled once for both; the
# resources are left out, since neither needs them.
set(_program_sources)
foreach(_source ${COMMON_SOURCES})
    # (the generated resources are the one absolute path)
    if(NOT IS_ABSOLUTE "${_source}")
        list(APPEND _program_sources ${CMAKE_SOURCE_DIR}/${_source})
    endif()
endforeach()
add_library(disparpdf_program_objects OBJECT ${_program_sources})
target_include_directories(disparpdf_program_objects PUBLIC
    ${CMAKE_SOURCE_DIR})
target_compile_definitions(disparpdf_program_objects PUBLIC
    POPPLER_VERSION="${POPPLER_VERSION_STR}"
    DISPARPDF_VERSION="${PROJECT_VERSION}")
target_link_libraries(disparpdf_program_objects PUBLIC ${QT_LIBS}
    ${POPPLER_TARGET} disparpdf_qpdf)
foreach(_test rendertest guitest)
    add_executable(${_test} ${_test}.cpp)
    target_link_libraries(${_test} PRIVATE disparpdf_program_objects)
    add_test(NAME ${_test}
             COMMAND ${_test} ${CMAKE_CURRENT_SOURCE_DIR}/data)
endforeach()
set_tests_properties(rendertest guitest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)

# The original batch-mode tests (testbase.sh, using ../data)
add_test(NAME batch-legacy
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/testbase.sh
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(batch-legacy PROPERTIES ENVIRONMENT
    "diffcmd=${DISPARPDFC};QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config")

# The others run disparPDFc on the PDFs in data/ (see data/README) and
# check the result code it prints and how many pages of differences it
# saves (see runtest.cpp)
add_executable(runtest runtest.cpp)
target_link_libraries(runtest PRIVATE ${QT_LIBS} ${POPPLER_TARGET})

set(DATA ${CMAKE_CURRENT_SOURCE_DIR}/data)

# disparpdf_test(NAME RESULT PAGES [OPTION]... -- [ARGUMENT]...)
function(disparpdf_test name result pages)
    add_test(NAME ${name}
             COMMAND runtest ${DISPARPDFC} ${result} ${pages}
                     ${ARGN})
endfunction()

# Identical content
foreach(mode appearance characters words)
    disparpdf_test(identical-${mode} 0 0 -- --${mode}
                   ${DATA}/base.pdf ${DATA}/same.pdf)
endforeach()

# One word changed, on page 2
foreach(mode appearance characters words)
    disparpdf_test(word-${mode} 1 1 -- --${mode}
                   ${DATA}/base.pdf ${DATA}/word.pdf)
endforeach()

# Same words, broken into lines differently: only the appearance differs
disparpdf_test(reflow-words 0 0 -- --words
               ${DATA}/base.pdf ${DATA}/reflow.pdf)
disparpdf_test(reflow-appearance 1 1 -- --appearance
               ${DATA}/base.pdf ${DATA}/reflow.pdf)

# Same lines in the same places, drawn in a different order
disparpdf_test(reordered-words 0 0 -- --words
               ${DATA}/base.pdf ${DATA}/reordered.pdf)
disparpdf_test(reordered-appearance 0 0 -- --appearance
               ${DATA}/base.pdf ${DATA}/reordered.pdf)

# The same page before and after rewriting it with Ghostscript, which
# changes Poppler's word order
foreach(mode characters words)
    disparpdf_test(rewritten-${mode} 0 0 -- --${mode}
                   ${DATA}/rewritten-original.pdf ${DATA}/rewritten-gs.pdf)
endforeach()

# A line drawn twice looks bolder
disparpdf_test(bold-words 0 0 -- --words ${DATA}/base.pdf ${DATA}/bold.pdf)
disparpdf_test(bold-appearance 1 1 -- --appearance
               ${DATA}/base.pdf ${DATA}/bold.pdf)

# A box 1.5in from the top of page 1, and one 0.25in from the top
disparpdf_test(box-words 0 0 -- --words ${DATA}/base.pdf ${DATA}/box.pdf)
disparpdf_test(box-appearance 1 1 -- --appearance
               ${DATA}/base.pdf ${DATA}/box.pdf)
disparpdf_test(marginbox-appearance 1 1 -- --appearance
               ${DATA}/base.pdf ${DATA}/marginbox.pdf)

# Excluding a 1in top margin hides only the box inside it, at any zoom
foreach(zoom 100 200)
    set(margins --setting Margins/Exclude=true --setting Margins/Top=72
                --setting Zoom=${zoom})
    disparpdf_test(margins-zoom${zoom}-box 1 1 ${margins} -- --appearance
                   ${DATA}/base.pdf ${DATA}/box.pdf)
    disparpdf_test(margins-zoom${zoom}-marginbox 0 0 ${margins} --
                   --appearance ${DATA}/base.pdf ${DATA}/marginbox.pdf)
endforeach()

# A page inserted after page 1: the page counts differ, but pages 2-3
# match pages 3-4
disparpdf_test(inserted 2 - -- --words ${DATA}/base.pdf ${DATA}/inserted.pdf)
disparpdf_test(inserted-start-pages 0 0 -- --words --startPage1=2
               --startPage2=3 --pages=2 ${DATA}/base.pdf ${DATA}/inserted.pdf)

# File names must end in .pdf unless --any-extension is given
configure_file(${DATA}/base.pdf ${CMAKE_CURRENT_BINARY_DIR}/base.dat COPYONLY)
disparpdf_test(extension-required -1 0 -- --words
               ${CMAKE_CURRENT_BINARY_DIR}/base.dat ${DATA}/same.pdf)
disparpdf_test(any-extension-same 0 0 -- --words --any-extension
               ${CMAKE_CURRENT_BINARY_DIR}/base.dat ${DATA}/same.pdf)
disparpdf_test(any-extension-word 1 1 -- --words --any-extension
               ${CMAKE_CURRENT_BINARY_DIR}/base.dat ${DATA}/word.pdf)

# The ConfrontaPDF test documents in ../data (see data/README): the
# comparisons batch-legacy makes by appearance, in every mode, also
# checking the pages of differences saved
set(CPDF ${CMAKE_SOURCE_DIR}/data)
foreach(mode appearance characters words)
    disparpdf_test(confrontapdf-reexported-${mode} 0 0 -- --${mode}
                   ${CPDF}/doca1pag.pdf ${CPDF}/doca1pag_dup.pdf)
    disparpdf_test(confrontapdf-1page-${mode} 1 1 -- --${mode}
                   ${CPDF}/doca1pag.pdf ${CPDF}/docb1pag.pdf)
    disparpdf_test(confrontapdf-2pages-${mode} 1 2 -- --${mode}
                   ${CPDF}/doca2pag.pdf ${CPDF}/docb2pag.pdf)
endforeach()
# The same text, 2 and 3 pages long: the page counts differ, but the
# first two pages are the same
disparpdf_test(confrontapdf-longer-words 2 - -- --words
               ${CPDF}/doca2pag.pdf ${CPDF}/doca3pag.pdf)
disparpdf_test(confrontapdf-longer-2pages-words 0 0 -- --words --pages=2
               ${CPDF}/doca2pag.pdf ${CPDF}/doca3pag.pdf)
# These differ only in the fonts they declare
disparpdf_test(confrontapdf-fonts-words 0 0 -- --words
               ${CPDF}/doca1pagi1.pdf ${CPDF}/doca1pagi2.pdf)
disparpdf_test(confrontapdf-compare-fonts 6 - -- --words --compareFonts
               ${CPDF}/doca1pagi1.pdf ${CPDF}/doca1pagi2.pdf)

# Comparing inside the trim box (see data/make-test-pdfs.sh)
foreach(mode appearance characters words)
    # The two differ only in a mark outside the trim box, which only an
    # appearance comparison of whole pages sees
    if(mode STREQUAL appearance)
        disparpdf_test(trim-mark-${mode} 1 1 -- --${mode}
                       ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)
    endif()
    disparpdf_test(box-trim-mark-${mode} 0 0 -- --${mode} --box=trim
                   ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)
    # A word inside it differs, and that is found and saved
    disparpdf_test(box-trim-word-${mode} 1 1 -- --${mode} --box=trim
                   ${DATA}/trim.pdf ${DATA}/trim-word.pdf)
    # With and without bleed: the trim boxes are lined up by their
    # corners, though plain.pdf's is its default, the whole page
    disparpdf_test(box-trim-bleed-${mode} 0 0
                   --stderr "file 1 has no TrimBox on page(s) 1"
                   --stderr "the TrimBox differs between the two files"
                   -- --${mode} --box=trim
                   ${DATA}/plain.pdf ${DATA}/bleed.pdf)
endforeach()
disparpdf_test(trim-bleed-appearance 1 1 -- --appearance
               ${DATA}/plain.pdf ${DATA}/bleed.pdf)
# The settings file can choose the box, and --box overrides it, also to
# compare whole pages
disparpdf_test(box-trim-setting 0 0 --setting PageBox/Box=trim --
               --appearance ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)
disparpdf_test(box-none-overrides-setting 1 1 --setting PageBox/Box=trim --
               --appearance --box=none ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)
# The media box takes in the mark, and so, clipped to it, does the bleed
# box, which defaults to the media box here
disparpdf_test(box-media-mark 1 1 -- --appearance --box=media
               ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)
disparpdf_test(box-bleed-mark 1 1 --stderr "has no BleedBox" -- --appearance
               --box=bleed ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)
# Margins are measured from the box: the changed word's line is 191pt
# below the top of the trim box and 227pt below the top of the page, so a
# 200pt top margin hides it only inside the trim box
set(margins --setting Margins/Exclude=true --setting Margins/Top=200)
disparpdf_test(box-trim-margins 0 0 ${margins} -- --appearance --box=trim
               ${DATA}/trim.pdf ${DATA}/trim-word.pdf)
disparpdf_test(trim-margins 1 1 ${margins} -- --appearance
               ${DATA}/trim.pdf ${DATA}/trim-word.pdf)
# Page 6 of boxes.pdf has a BleedBox outside its CropBox and a TrimBox
# outside its MediaBox (compared with a copy, since a file is not compared
# with itself)
configure_file(${DATA}/boxes.pdf ${CMAKE_CURRENT_BINARY_DIR}/boxes-copy.pdf
               COPYONLY)
disparpdf_test(box-bleed-beyond-crop 0 0
               --stderr "on page(s) 6 of file 1, the BleedBox extends past the CropBox"
               --stderr "on page(s) 6 of file 2, the BleedBox extends past the CropBox"
               -- --appearance --box=bleed --box-clip=crop
               ${DATA}/boxes.pdf ${CMAKE_CURRENT_BINARY_DIR}/boxes-copy.pdf)
disparpdf_test(box-trim-beyond-media 0 0
               --stderr "on page(s) 6 of file 1, the TrimBox extends past the MediaBox"
               --stderr "file 1 has no TrimBox on page(s) 5"
               -- --appearance --box=trim --box-clip=none
               ${DATA}/boxes.pdf ${CMAKE_CURRENT_BINARY_DIR}/boxes-copy.pdf)
# --box-clip alone does nothing but say so
disparpdf_test(box-clip-without-box 1 1
               --stderr "--box-clip is ignored without --box" --
               --appearance --box-clip=none ${DATA}/trim.pdf
               ${DATA}/trim-mark.pdf)
disparpdf_test(invalid-box -1 0 -- --box=page
               ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)
disparpdf_test(invalid-box-clip -1 0 -- --box=trim --box-clip=trim
               ${DATA}/trim.pdf ${DATA}/trim-mark.pdf)

# The XML result names the files compared, not their directories
disparpdf_test(xml-file-names 1 1 --xml "/base.pdf\" pageCount="
               --xml "/word.pdf\" pageCount=" --
               --words ${DATA}/base.pdf ${DATA}/word.pdf)

# Command line handling
disparpdf_test(same-file 5 0 -- --words ${DATA}/base.pdf ${DATA}/base.pdf)
disparpdf_test(missing-file -4 0 -- ${DATA}/base.pdf ${DATA}/no-such.pdf)
disparpdf_test(one-file -1 0 -- ${DATA}/base.pdf)
disparpdf_test(invalid-pages -1 0 -- --pages=x
               ${DATA}/base.pdf ${DATA}/same.pdf)

# The GUI program in batch mode, which must manage without a display
add_test(NAME gui-program-batch
         COMMAND runtest $<TARGET_FILE:disparPDF> 1 1 -- --words
                 ${DATA}/base.pdf ${DATA}/word.pdf)
add_test(NAME gui-program-help
         COMMAND runtest $<TARGET_FILE:disparPDF> - - --stdout
                 "Usage: disparPDF " -- --help)

# Run by the name disparPDFc, the program is in batch mode without -b,
# whatever the case of the name and with or without .exe
add_test(NAME console-by-name
         COMMAND ${DISPARPDFC} --words ${DATA}/base.pdf ${DATA}/word.pdf)
add_test(NAME console-by-name-case
         COMMAND sh -c "dir=$(mktemp -d) && ln -s \"$1\" \"$dir/DisparPDFC.exe\" && \"$dir/DisparPDFC.exe\" --words \"$2\" \"$3\"; status=$?; rm -r \"$dir\"; exit $status"
                 sh $<TARGET_FILE:disparPDF> ${DATA}/base.pdf ${DATA}/word.pdf)
# ... unless given --interactive, though the last of that and --batch wins
add_test(NAME console-interactive-batch
         COMMAND ${DISPARPDFC} --interactive --batch --words
                 ${DATA}/base.pdf ${DATA}/word.pdf)
set_tests_properties(console-by-name console-by-name-case
                     console-interactive-batch PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config"
    PASS_REGULAR_EXPRESSION "(^|\n)1\n" TIMEOUT 120)

# With --interactive it shows the window (on Qt's offscreen platform, here)
# and keeps running, until timeout(1) stops it, instead of printing the
# result and exiting
find_program(TIMEOUT_EXECUTABLE timeout)
if(TIMEOUT_EXECUTABLE)
    add_test(NAME console-interactive
             COMMAND sh -c "\"$0\" 10 \"$@\"; test $? = 124"
                     ${TIMEOUT_EXECUTABLE} ${DISPARPDFC} --interactive --words
                     ${DATA}/base.pdf ${DATA}/word.pdf)
    set_tests_properties(console-interactive PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config")
endif()

disparpdf_test(help - - --stdout "Usage: disparPDFc" -- --help)
disparpdf_test(version - - --stdout "disparPDFc ${PROJECT_VERSION}" -- --version)

# Shell completions: every option in --help is offered, and they work
# (these two are skipped without bash-completion or zsh)
add_test(NAME completion-options
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/completion-options.sh
                 $<TARGET_FILE:disparPDF>
                 ${CMAKE_SOURCE_DIR}/completions/disparPDF.bash
                 ${CMAKE_SOURCE_DIR}/completions/_disparPDF)
add_test(NAME completion-options-c
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/completion-options.sh
                 ${DISPARPDFC}
                 ${CMAKE_SOURCE_DIR}/completions/disparPDF.bash
                 ${CMAKE_SOURCE_DIR}/completions/_disparPDF)
add_test(NAME completion-bash
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/completion-bash.sh
                 ${CMAKE_SOURCE_DIR}/completions/disparPDF.bash)
add_test(NAME completion-zsh
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/completion-zsh.sh
                 ${CMAKE_SOURCE_DIR}/completions)
set_tests_properties(completion-options completion-options-c
                     completion-bash completion-zsh
                     PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(completion-bash completion-zsh
                     PROPERTIES SKIP_RETURN_CODE 77)
