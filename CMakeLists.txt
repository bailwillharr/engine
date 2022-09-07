cmake_minimum_required(VERSION 3.4)

# options

project(engine LANGUAGES CXX
	VERSION "0.1.0"
)

add_library(${PROJECT_NAME} SHARED
	
	"src/window.cpp"

	"src/input.cpp"	#TODO make input_manager

	"src/object.cpp"
	"src/sceneroot.cpp"

	"src/components/component.cpp"
	 # TODO move functionality into "Object" class
	"src/components/camera.cpp"
	"src/components/mesh_renderer.cpp"
	"src/components/text_ui_renderer.cpp"
	"src/components/custom.cpp"

	"src/resources/resource.cpp"
	"src/resources/mesh.cpp"
	"src/resources/shader.cpp"
	"src/resources/texture.cpp"
	"src/resources/font.cpp"

	"src/resource_manager.cpp"

	# PUBLIC API

	"include/engine_api.h"

	"include/engine.hpp"

	"include/log.hpp"

	"include/window.hpp"

	"include/inputs/keyboard.hpp"
	"include/inputs/mouse.hpp"

	"include/input.hpp"

	"include/transform.hpp"
	"include/object.hpp"
	"include/sceneroot.hpp"

	"include/components/component.hpp"
	"include/components/camera.hpp"
	"include/components/mesh_renderer.hpp"
	"include/components/text_ui_renderer.hpp"
	"include/components/custom.hpp"

	"include/resources/resource.hpp"
	"include/resources/mesh.hpp"
	"include/resources/shader.hpp"
	"include/resources/texture.hpp"
	"include/resources/font.hpp"

	"include/resource_manager.hpp"

)

# compiling options:

target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "ENGINE_EXPORTS")

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /MP)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Pass some project information into the source code
configure_file(config.h.in config.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# project submodules:

add_subdirectory(graphics)
target_include_directories(${PROJECT_NAME} PUBLIC graphics/include)
target_link_libraries(${PROJECT_NAME} PUBLIC graphics)



# external libraries:

# MinGW library if using it
if (MINGW)
	target_link_libraries(${PROJECT_NAME} PUBLIC mingw32)
endif()

# SDL2:
set(SDL2_DISABLE_INSTALL ON CACHE INTERNAL "" FORCE)
set(SDL_SHARED ON CACHE INTERNAL "" FORCE)
set(SDL_STATIC OFF CACHE INTERNAL "" FORCE)
set(SDL_TEST OFF CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(dependencies/SDL)
target_include_directories(${PROJECT_NAME} PUBLIC dependencies/SDL/include)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2main)

# GLM:
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dependencies/glm)
target_include_directories(${PROJECT_NAME} PUBLIC dependencies/glm)

# GLAD:
set(GLAD_PROFILE "core" CACHE INTERNAL "" FORCE)
set(GLAD_API "gl=3.3" CACHE INTERNAL "" FORCE)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(GLAD_GENERATOR "c-debug" CACHE INTERNAL "" FORCE)
else()
	set(GLAD_GENERATOR "c" CACHE INTERNAL "" FORCE)
endif()
set(GLAD_SPEC "gl" CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dependencies/glad)
set_property(TARGET glad PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} PUBLIC glad)
target_include_directories(${PROJECT_NAME} PUBLIC dependencies/glad/include)

# spdlog
set(SPDLOG_BUILD_SHARED ON CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(dependencies/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)
target_include_directories(${PROJECT_NAME} PUBLIC dependencies/spdlog/include)

# freetype
set(FT_DISABLE_ZLIB TRUE CACHE INTERNAL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE INTERNAL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE INTERNAL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE INTERNAL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(dependencies/freetype)
target_link_libraries(${PROJECT_NAME} PRIVATE freetype)
target_include_directories(${PROJECT_NAME} PRIVATE dependencies/freetype/include)

# stb
target_include_directories(${PROJECT_NAME} PRIVATE dependencies/stb)
