cmake_minimum_required(VERSION 3.24)

project(PerlinNoise LANGUAGES CXX)

set(INCLUDE_FILES
	"include/PerlinNoise.hpp"
)

add_library(${PROJECT_NAME} INTERFACE
	${INCLUDE_FILES}
)

target_include_directories(${PROJECT_NAME} INTERFACE include)