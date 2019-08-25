# if (DOXYGEN_FOUND)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/docs)

    set(doxyfile_in ${CMAKE_SOURCE_DIR}/Doxyfile)
    set(doxyfile ${CMAKE_BINARY_DIR}/docs/Doxyfile)

    configure_file(${doxyfile_in} ${doxyfile} @ONLY)

    add_custom_target(
        golfl-doc ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile_in}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
# endif()

file(GLOB GOLFL_SRC *.c)
add_library(golfl STATIC ${GOLFL_SRC})
target_include_directories(golfl PUBLIC ../include)
