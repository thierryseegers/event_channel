cmake_minimum_required(VERSION 3.1)

project(event_channel)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 17)

enable_testing()

add_subdirectory(documentation)
add_subdirectory(examples)
add_subdirectory(include)
add_subdirectory(tests)

install(FILES ${PROJECT_SOURCE_DIR}/include/event_channel.h DESTINATION include)
install(DIRECTORY ${PROJECT_BINARY_DIR}/documentation/htdocs DESTINATION documentation)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}_${VERSION_MAJOR}_${VERSION_MINOR})
include(CPack)
