cmake_minimum_required(VERSION 3.24)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# options
option(ENGINE_BUILD_TEST "Compile the test program" ON)
if (MSVC)
	option(ENGINE_HOT_RELOAD "Enable VS hot reload" OFF)
endif()
option(ENGINE_PHYSX_INCLUDE_DIR "Path to the PhysX SDK include directory" "")
option(ENGINE_PHYSX_LIBRARY_DIR "PhysX library location (often different per build configuration)" "")

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")

# enable link time code generation for all targets in the solution
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

project(engine LANGUAGES CXX C
	VERSION "0.2.0"
)

# from command: find . -regex "^\.\/.*" | sort
set(SRC_FILES
	"src/application.cpp"
	"src/application_component.cpp"
	"src/ecs.cpp"
	"src/file_dialog.cpp"
	"src/files.cpp"
	"src/gen_tangents.cpp"
	"src/gfx_device.cpp"
	"src/gltf_loader.cpp"
	"src/input_manager.cpp"
	"src/renderer.cpp"
	"src/resource_font.cpp"
	"src/resource_material.cpp"
	"src/resource_mesh.cpp"
	"src/resource_shader.cpp"
	"src/resource_texture.cpp"
	"src/scene.cpp"
	"src/scene_manager.cpp"
	"src/system_collisions.cpp"
	"src/system_custom_behaviour.cpp"
	"src/system_mesh_render.cpp"
	"src/system_transform.cpp"
	"src/vulkan_allocator.cpp"
	"src/vulkan_device.cpp"
	"src/vulkan_instance.cpp"
	"src/vulkan_swapchain.cpp"
	"src/window.cpp"
	"src/physics.cpp"
)

set(INCLUDE_FILES
	"include/application.h"
	"include/application_component.h"
	"include/component_collider.h"
	"include/component_custom.h"
	"include/component_mesh.h"
	"include/component_transform.h"
	"include/debug_line.h"
	"include/ecs.h"
	"include/entity.h"
	"include/event_system.h"
	"include/file_dialog.h"
	"include/files.h"
	"include/gen_tangents.h"
	"include/gfx.h"
	"include/gfx_device.h"
	"include/gltf_loader.h"
	"include/input_keys.h"
	"include/input_manager.h"
	"include/input_mouse.h"
	"include/log.h"
	"include/logger.h"
	"include/renderer.h"
	"include/resource_font.h"
	"include/resource_manager.h"
	"include/resource_material.h"
	"include/resource_mesh.h"
	"include/resource_shader.h"
	"include/resource_texture.h"
	"include/scene.h"
	"include/scene_manager.h"
	"include/system_collisions.h"
	"include/system_custom_behaviour.h"
	"include/system_mesh_render.h"
	"include/system_transform.h"
	"include/util.h"
	"include/vulkan_allocator.h"
	"include/vulkan_device.h"
	"include/vulkan_instance.h"
	"include/vulkan_swapchain.h"
	"include/window.h"
	"include/physics.h"
)

add_library(${PROJECT_NAME} STATIC
	${SRC_FILES}
	${INCLUDE_FILES}
	${RES_FILES}
)

# show game engine resources in VS generated solution explorer
file(GLOB_RECURSE RES_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/res/engine/*")

# Organise files in VS generated solution explorer
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${SRC_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Include" FILES ${INCLUDE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/res/engine" PREFIX "Resources" FILES ${RES_FILES})

# compiling options:

if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "NOMINMAX") # stop windows.h conflicting with 'std::max'
endif()

# This project uses C++20 and C11
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

# compiler warnings
if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /MP)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

if (MSVC AND ENGINE_HOT_RELOAD)
	target_compile_options(${PROJECT_NAME} PRIVATE /ZI)
	target_link_options(${PROJECT_NAME} PRIVATE /INCREMENTAL)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Pass some project information into the source code
configure_file(config.h.in config.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Build the test
if (ENGINE_BUILD_TEST)
	add_subdirectory(test)
	# Copy DLLs to the enginetest.exe folder (I think that's what this does?)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:enginetest>)
	# Set enginetest.exe as the default startup project in VS
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT enginetest)
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
add_subdirectory(vendor/volk)
target_link_libraries(${PROJECT_NAME} PUBLIC volk::volk)

# Vulkan Memory Allocator
add_subdirectory(vendor/VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PUBLIC VulkanMemoryAllocator)

# shaderc
if (MSVC)
	include(FindVulkan)
	find_package(Vulkan COMPONENTS shaderc_combined)
	target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::shaderc_combined)
else()
	target_link_libraries(${PROJECT_NAME} PUBLIC shaderc_shared)
endif()

# SDL2:
set(SDL2_DISABLE_INSTALL ON CACHE INTERNAL "" FORCE)
set(SDL_SHARED ON CACHE INTERNAL "" FORCE)
set(SDL_STATIC OFF CACHE INTERNAL "" FORCE)
set(SDL_TEST OFF CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(vendor/SDL)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2main)

# GLM:
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(vendor/glm)
target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm)

# spdlog
set(SPDLOG_BUILD_SHARED OFF)
set(SPDLOG_SYSTEM_INCLUDES ON)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(vendor/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)

# stb
add_subdirectory(vendor/stb)
target_link_libraries(${PROJECT_NAME} PUBLIC stb)

# imgui (depends on volk, SDL2, and stb)
add_subdirectory(vendor/imgui)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui)

# json
add_subdirectory(vendor/json)
target_link_libraries(${PROJECT_NAME} PUBLIC json)

# tinygltf (depends on json and stb)
add_subdirectory(vendor/tinygltf)
target_link_libraries(${PROJECT_NAME} PUBLIC tinygltf)

# mikktspace
add_subdirectory(vendor/mikktspace)
target_link_libraries(${PROJECT_NAME} PUBLIC mikktspace)

# weldmesh
add_subdirectory(vendor/weldmesh)
target_link_libraries(${PROJECT_NAME} PUBLIC weldmesh)

# PerlinNoise
add_subdirectory(vendor/PerlinNoise)
target_link_libraries(${PROJECT_NAME} PUBLIC PerlinNoise)

# PhysX 5
target_include_directories(${PROJECT_NAME} PUBLIC ${ENGINE_PHYSX_INCLUDE_DIR})
target_link_directories(${PROJECT_NAME} PUBLIC ${ENGINE_PHYSX_LIBRARY_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC
	PhysXCooking_64
	PVDRuntime_64
	PhysX_64
	PhysXCommon_64
	PhysXFoundation_64
)