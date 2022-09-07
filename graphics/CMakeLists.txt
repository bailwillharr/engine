cmake_minimum_required(VERSION 3.4)

# options

project(graphics LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC

	"src/gfx_device.cpp"
	
	# PUBLIC API

	"include/gfx_api.h"
	"include/gfx.hpp"
	"include/gfx_device.hpp"

)

# compiling options:

target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "GFX_EXPORTS")

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# libraries:
