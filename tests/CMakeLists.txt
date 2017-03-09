find_package(Threads REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)

if(MSVC)
	add_definitions(/D _CRT_SECURE_NO_WARNINGS)
	add_definitions(/D _SCL_SECURE_NO_WARNINGS)
endif()

add_executable(correctness catch.hpp semaphore.hpp correctness.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
   CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	target_compile_options(correctness
		PUBLIC -std=c++1z
	)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(correctness
		PUBLIC /std:c++latest
	)
endif()

target_link_libraries(correctness Threads::Threads)

add_test(i_1_1_s correctness i_1_1_s)
add_test(s_1_1_s correctness s_1_1_s)
add_test(i_1_3_s correctness i_1_3_s)
add_test(i_3_1_s correctness i_3_1_s)
add_test(i_1_1_s correctness i_1_1_s)
add_test(i_1_1_p correctness i_1_1_p)

add_test(i_3_3_s correctness i_3_3_s)
add_test(i_3_3_p correctness i_3_3_p)
