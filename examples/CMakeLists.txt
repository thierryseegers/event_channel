find_package(Threads REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include) 

if(MSVC)
	add_definitions(/D _CRT_SECURE_NO_WARNINGS)
	add_definitions(/D _SCL_SECURE_NO_WARNINGS)
endif()

add_executable(example example.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
   CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	target_compile_options(example
		PUBLIC -std=c++1z
	)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(example
		PUBLIC /std:c++latest
	)
endif()

target_link_libraries(example Threads::Threads)
