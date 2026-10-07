# Tests, run with "ctest" in the build directory.  They need no display.

# The original batch-mode tests (testbase.sh, using ../data)
add_test(NAME batch-legacy
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/testbase.sh
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(batch-legacy PROPERTIES ENVIRONMENT
    "diffcmd=$<TARGET_FILE:disparPDFc>;QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config")

# The others run disparPDFc on the PDFs in data/ (see data/README) and
# check the result code it prints and how many pages of differences it
# saves (see runtest.cpp)
add_executable(runtest runtest.cpp)
target_link_libraries(runtest PRIVATE ${QT_LIBS} ${POPPLER_TARGET})

set(DATA ${CMAKE_CURRENT_SOURCE_DIR}/data)

# disparpdf_test(NAME RESULT PAGES [OPTION]... -- [ARGUMENT]...)
function(disparpdf_test name result pages)
    add_test(NAME ${name}
             COMMAND runtest $<TARGET_FILE:disparPDFc> ${result} ${pages}
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
                 $<TARGET_FILE:disparPDFc>
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
