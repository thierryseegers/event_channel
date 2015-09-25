find_package(Threads REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include) 

if(MSVC)
	add_definitions(/D _CRT_SECURE_NO_WARNINGS)
	add_definitions(/D _SCL_SECURE_NO_WARNINGS)
endif()

add_executable(example example.cpp)

set_target_properties(example PROPERTIES
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON)

target_link_libraries(example Threads::Threads)
