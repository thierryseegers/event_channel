find_package(Threads REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include) 

if(MSVC)
	add_definitions(/D _CRT_SECURE_NO_WARNINGS)
	add_definitions(/D _SCL_SECURE_NO_WARNINGS)
endif()

add_executable(example example.cpp)

# Revisit when CMake supports C++17
#set_target_properties(correctness PROPERTIES
#    FOLDER "tests"
#	CXX_STANDARD 14
#	CXX_STANDARD_REQUIRED ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
   CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	target_compile_options(example
		PUBLIC -std=c++1z
	)
endif()

target_link_libraries(example Threads::Threads)
