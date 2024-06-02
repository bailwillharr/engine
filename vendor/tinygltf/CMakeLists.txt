cmake_minimum_required(VERSION 3.24)

project(tinygltf LANGUAGES CXX)

set(SRC_FILES
	"src/tiny_gltf_impl.cpp"
)

set(INCLUDE_FILES
	"include/tiny_gltf.h"
)

add_library(${PROJECT_NAME} STATIC
	${SRC_FILES}
	${INCLUDE_FILES}
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE json)
target_link_libraries(${PROJECT_NAME} PRIVATE stb)