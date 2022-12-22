cmake_minimum_required(VERSION 3.24)

# options
option(ENGINE_BUILD_TEST "Compile the test program" ON)
option(ENGINE_BUILD_VULKAN "Use Vulkan 1.3 for graphics" ON)

project(engine LANGUAGES CXX
	VERSION "0.1.0"
)

set(SRC_FILES
	"src/application.cpp"
	"src/window.cpp"

	"src/input_manager.cpp"
	"src/scene_manager.cpp"

	"src/resources/texture.cpp"
	"src/resources/material.cpp"

	"src/gfx_device_vulkan.cpp"

	"src/scene.cpp"

	"src/util/files.cpp"
)

set(INCLUDE_FILES
	"include/engine_api.h"
	"include/application.hpp"
	"include/util.hpp"
	"include/log.hpp"
	"include/logger.hpp"
	"include/window.hpp"
	"include/inputs/keyboard.hpp"
	"include/inputs/mouse.hpp"

	"include/input_manager.hpp"
	"include/scene_manager.hpp"

	"include/resource_manager.hpp"
	"include/resources/material.hpp"
	"include/resources/texture.hpp"
	"include/resources/shader.hpp"
	"include/resources/mesh.hpp"

	"include/gfx.hpp"
	"include/gfx_device.hpp"

	"include/scene.hpp"
	"include/ecs/ecs_system.hpp"
	"include/ecs/transform.hpp"
	"include/ecs/mesh_renderer.hpp"

	"include/util/files.hpp"
)

add_library(${PROJECT_NAME} STATIC
	${SRC_FILES}
	${INCLUDE_FILES}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${SRC_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Include" FILES ${INCLUDE_FILES})

# compiling options:

target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "ENGINE_EXPORTS")

set_property(TARGET ${PROJECT_NAME} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /MP)

	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Pass some project information into the source code
configure_file(config.h.in config.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# figure out what graphics api to use
if (ENGINE_BUILD_VULKAN)
	target_compile_definitions(${PROJECT_NAME} PRIVATE "ENGINE_BUILD_VULKAN")
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE "ENGINE_BUILD_NULL")
endif()

# Build the test
if (ENGINE_BUILD_TEST)
	add_subdirectory(test)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:enginetest>)
endif()

# external libraries:

# MinGW library if using it
if (MINGW)
	target_link_libraries(${PROJECT_NAME} PUBLIC mingw32)
endif()

if(ENGINE_BUILD_OPENGL)
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
	target_link_libraries(${PROJECT_NAME} PUBLIC glad)
	target_include_directories(${PROJECT_NAME} PUBLIC dependencies/glad/include)
endif()

if(ENGINE_BUILD_VULKAN)
	# Volk
	set(VOLK_STATIC_DEFINES "")
	set(VOLK_PULL_IN_VULKAN ON)
	set(VOLK_INSTALL OFF)
	set(VOLK_HEADERS_ONLY ON)
	add_subdirectory(dependencies/volk)
	target_link_libraries(${PROJECT_NAME} PRIVATE volk_headers)
	# Vulkan Memory Allocator
	target_include_directories(${PROJECT_NAME} PRIVATE dependencies/VulkanMemoryAllocator/include)
	# shaderc
	if (MSVC)
		include(FindVulkan)
		find_package(Vulkan COMPONENTS shaderc_combined)
		target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::shaderc_combined)
	else()
		target_link_libraries(${PROJECT_NAME} PRIVATE shaderc_shared)
	endif()
endif()

# SDL2:
find_package(SDL2)
if (SDL2_FOUND)
	target_include_directories(${PROJECT_NAME} PUBLIC ${SDL2_INCLUDE_DIRS})
else()
	set(SDL2_DISABLE_INSTALL ON CACHE INTERNAL "" FORCE)
	set(SDL_SHARED ON CACHE INTERNAL "" FORCE)
	set(SDL_STATIC OFF CACHE INTERNAL "" FORCE)
	set(SDL_TEST OFF CACHE INTERNAL "" FORCE)
	set(BUILD_SHARED_LIBS ON)
	add_subdirectory(dependencies/SDL)
	target_include_directories(${PROJECT_NAME} PUBLIC dependencies/SDL/include)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2main)

# GLM:
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dependencies/glm)
target_include_directories(${PROJECT_NAME} PUBLIC dependencies/glm)

# spdlog
set(SPDLOG_BUILD_SHARED OFF CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dependencies/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)
target_include_directories(${PROJECT_NAME} PUBLIC dependencies/spdlog/include)

# freetype
#set(FT_DISABLE_ZLIB TRUE CACHE INTERNAL "" FORCE)
#set(FT_DISABLE_BZIP2 TRUE CACHE INTERNAL "" FORCE)
#set(FT_DISABLE_PNG TRUE CACHE INTERNAL "" FORCE)
#set(FT_DISABLE_HARFBUZZ TRUE CACHE INTERNAL "" FORCE)
#set(FT_DISABLE_BROTLI TRUE CACHE INTERNAL "" FORCE)
#set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
#add_subdirectory(dependencies/freetype)
#target_link_libraries(${PROJECT_NAME} PRIVATE freetype)
#target_include_directories(${PROJECT_NAME} PRIVATE dependencies/freetype/include)

# stb
target_include_directories(${PROJECT_NAME} PRIVATE dependencies/stb)

# assimp
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE INTERNAL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE INTERNAL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(dependencies/assimp)
target_include_directories(${PROJECT_NAME} PRIVATE dependencies/assimp/include)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
