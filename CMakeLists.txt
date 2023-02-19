cmake_minimum_required(VERSION 3.24)

# options
option(ENGINE_BUILD_TEST "Compile the test program" ON)
option(ENGINE_BUILD_VULKAN "Use Vulkan 1.3 for graphics" ON)

project(engine LANGUAGES CXX
	VERSION "0.1.0"
)

set(SRC_FILES
	src/ecs_system.cpp
	src/application.cpp

	src/libs/stb_image.cpp

	src/systems/transform.cpp
	src/systems/render.cpp
	src/systems/collisions.cpp

	src/resources/shader.cpp
	src/resources/material.cpp
	src/resources/mesh.cpp
	src/resources/texture.cpp

	src/scene.cpp

	src/gfx_device_vulkan.cpp
	src/gfx_device_null.cpp

	src/util/files.cpp
	src/util/model_loader.cpp

	src/scene_manager.cpp
	src/input_manager.cpp
	src/window.cpp
)

set(INCLUDE_FILES
	include/ecs_system.hpp
	include/gfx_device.hpp

	include/inputs/mouse.hpp
	include/inputs/keyboard.hpp
	include/input_manager.hpp

	include/systems/transform.hpp
	include/systems/render.hpp
	include/systems/collisions.hpp

	include/resources/shader.hpp
	include/resources/material.hpp
	include/resources/mesh.hpp
	include/resources/texture.hpp

	include/event_system.hpp

	include/engine_api.h

	include/util/files.hpp
	include/util/model_loader.hpp

	include/util.hpp
	include/logger.hpp
	include/scene_manager.hpp
	include/gfx.hpp
	include/scene.hpp
	include/window.hpp
	include/log.hpp
	include/resource_manager.hpp
	include/application.hpp

	include/components/transform.hpp
	include/components/renderable.hpp
	include/components/collider.hpp
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
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /MP)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -pedantic)
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

if(ENGINE_BUILD_VULKAN)
	# Volk
	set(VOLK_STATIC_DEFINES "")
	set(VOLK_PULL_IN_VULKAN ON)
	set(VOLK_INSTALL OFF)
	set(VOLK_HEADERS_ONLY OFF)
	add_subdirectory(dependencies/volk)
	target_link_libraries(${PROJECT_NAME} PRIVATE volk::volk)
	# Vulkan Memory Allocator
	target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE dependencies/VulkanMemoryAllocator/include)
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
	target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
else()
	set(SDL2_DISABLE_INSTALL ON CACHE INTERNAL "" FORCE)
	set(SDL_SHARED ON CACHE INTERNAL "" FORCE)
	set(SDL_STATIC OFF CACHE INTERNAL "" FORCE)
	set(SDL_TEST OFF CACHE INTERNAL "" FORCE)
	set(BUILD_SHARED_LIBS ON)
	add_subdirectory(dependencies/SDL)
	target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC dependencies/SDL/include)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2main)

# GLM:
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dependencies/glm)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC dependencies/glm)

# spdlog
set(SPDLOG_BUILD_SHARED OFF CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dependencies/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC dependencies/spdlog/include)

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
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE dependencies/stb)

# assimp
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE INTERNAL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE INTERNAL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE INTERNAL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE INTERNAL "" FORCE)
add_subdirectory(dependencies/assimp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE dependencies/assimp/include)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
