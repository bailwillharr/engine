cmake_minimum_required(VERSION 3.24)

# options
option(ENGINE_BUILD_TEST "Compile the test program" ON)

SET(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")

project(engine LANGUAGES CXX
	VERSION "0.1.0"
)

# from command: find . -regex "^\.\/.*" | sort
set(SRC_FILES
	"src/application.cpp"
	"src/ecs_system.cpp"
	"src/gfx_device_vulkan.cpp"
	"src/input_manager.cpp"
	"src/libs/stb_image.cpp"
	"src/libs/stb_truetype.cpp"
	"src/resources/font.cpp"
	"src/resources/material.cpp"
	"src/resources/mesh.cpp"
	"src/resources/shader.cpp"
	"src/resources/texture.cpp"
	"src/scene.cpp"
	"src/scene_manager.cpp"
	"src/systems/collisions.cpp"
	"src/systems/render.cpp"
	"src/systems/render2d.cpp"
	"src/systems/transform.cpp"
	"src/systems/ui.cpp"
	"src/util/files.cpp"
	"src/util/model_loader.cpp"
	"src/vulkan/device.cpp"
	"src/vulkan/device.h"
	"src/vulkan/gpu_allocator.cpp"
	"src/vulkan/gpu_allocator.h"
	"src/vulkan/instance.cpp"
	"src/vulkan/instance.h"
	"src/vulkan/swapchain.cpp"
	"src/vulkan/swapchain.h"
	"src/window.cpp"
)

set(INCLUDE_FILES
	"include/application.h"
	"include/components/collider.h"
	"include/components/renderable_text.h"
	"include/components/renderable.h"
	"include/components/transform.h"
	"include/components/ui_element.h"
	"include/ecs_system.h"
	"include/engine_api.h"
	"include/event_system.h"
	"include/gfx.h"
	"include/gfx_device.h"
	"include/input_manager.h"
	"include/inputs/keyboard.h"
	"include/inputs/mouse.h"
	"include/log.h"
	"include/logger.h"
	"include/resource_manager.h"
	"include/resources/font.h"
	"include/resources/material.h"
	"include/resources/mesh.h"
	"include/resources/shader.h"
	"include/resources/texture.h"
	"include/scene.h"
	"include/scene_manager.h"
	"include/systems/collisions.h"
	"include/systems/render.h"
	"include/systems/render2d.h"
	"include/systems/transform.h"
	"include/systems/ui.h"
	"include/util.h"
	"include/util/files.h"
	"include/util/model_loader.h"
	"include/window.h"
)

add_library(${PROJECT_NAME} STATIC
	${SRC_FILES}
	${INCLUDE_FILES}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${SRC_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Include" FILES ${INCLUDE_FILES})

# compiling options:

target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "ENGINE_EXPORTS")

if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "NOMINMAX") # stop windows.h conflicting with 'std::max'
endif()

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

# Build the test
if (ENGINE_BUILD_TEST)
	add_subdirectory(test)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:enginetest>)
endif()

# private libraries:

# Volk
set(VOLK_STATIC_DEFINES "")
set(VOLK_PULL_IN_VULKAN ON)
set(VOLK_INSTALL OFF)
set(VOLK_HEADERS_ONLY OFF)
if (WIN32)
	set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()
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

# public libraries:

# SDL2:
set(SDL2_DISABLE_INSTALL ON CACHE INTERNAL "" FORCE)
set(SDL_SHARED ON CACHE INTERNAL "" FORCE)
set(SDL_STATIC OFF CACHE INTERNAL "" FORCE)
set(SDL_TEST OFF CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(dependencies/SDL)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC dependencies/SDL/include)
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
