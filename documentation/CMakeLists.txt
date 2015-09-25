find_package(Doxygen REQUIRED)

if(DOXYGEN_FOUND)
    set(DOXYGEN_RAW ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_CONFIGURED ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    set(DOCUMENTATION_INDEX_FILE ${CMAKE_CURRENT_BINARY_DIR}/htdocs/index.html)

    configure_file(${DOXYGEN_RAW} ${DOXYGEN_CONFIGURED})

    add_custom_command(
        OUTPUT ${DOCUMENTATION_INDEX_FILE}
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIGURED}
DEPENDS ${DOXYGEN_CONFIGURED} ${CMAKE_CURRENT_SOURCE_DIR}/mainpage.h ${PROJECT_SOURCE_DIR}/include/event_channel.h
    )

    add_custom_target(documentation ALL
        DEPENDS ${DOCUMENTATION_INDEX_FILE}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
        SOURCES ${DOXYGEN_RAW} ${CMAKE_CURRENT_SOURCE_DIR}/mainpage.h)

endif()
