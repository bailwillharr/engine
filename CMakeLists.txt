cmake_minimum_required(VERSION 3.24)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# options
option(ENGINE_BUILD_TEST "Compile the test program" ON)

SET(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")

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
	"src/gfx_device_vulkan.cpp"
	"src/imgui/imconfig.h"
	"src/imgui/imgui.cpp"
	"src/imgui/imgui.h"
	"src/imgui/imgui_demo.cpp"
	"src/imgui/imgui_draw.cpp"
	"src/imgui/imgui_impl_sdl2.cpp"
	"src/imgui/imgui_impl_sdl2.h"
	"src/imgui/imgui_impl_vulkan.cpp"
	"src/imgui/imgui_impl_vulkan.h"
	"src/imgui/imgui_internal.h"
	"src/imgui/imgui_tables.cpp"
	"src/imgui/imgui_widgets.cpp"
	"src/imgui/imstb_textedit.h"
	"src/input_manager.cpp"
	"src/libs/json.hpp"
	"src/libs/mikktspace.c"
	"src/libs/mikktspace.h"
	"src/libs/stb_impl.cpp"
	"src/libs/tiny_gltf.h"
	"src/libs/tiny_gltf_impl.cpp"
	"src/libs/weldmesh.c"
	"src/libs/weldmesh.h"
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
	"src/files.cpp"
	"src/gen_tangents.cpp"
	"src/gltf_loader.cpp"
	"src/vulkan_device.cpp"
	"src/vulkan_allocator.cpp"
	"src/vulkan_instance.cpp"
	"src/vulkan_swapchain.cpp"
	"src/window.cpp"
	"src/file_dialog.cpp"
)

set(INCLUDE_FILES
	"include/application.h"
	"include/application_component.h"
	"include/component_collider.h"
	"include/component_custom.h"
	"include/component_mesh.h"
	"include/component_transform.h"
	"include/ecs.h"
	"include/vulkan_allocator.h"
	"include/vulkan_device.h"
	"include/vulkan_instance.h"
	"include/vulkan_swapchain.h"
	"include/event_system.h"
	"include/gfx.h"
	"include/gfx_device.h"
	"include/input_manager.h"
	"include/input_keys.h"
	"include/input_mouse.h"
	"include/log.h"
	"include/logger.h"
	"include/renderer.h"
	"include/resource_manager.h"
	"include/resource_font.h"
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
	"include/files.h"
	"include/gen_tangents.h"
	"include/gltf_loader.h"
	"include/window.h"
	"include/file_dialog.h"
)


file(GLOB_RECURSE RES_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/res/engine/*")

add_library(${PROJECT_NAME} STATIC
	${SRC_FILES}
	${INCLUDE_FILES}
	${RES_FILES}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${SRC_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Include" FILES ${INCLUDE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/res/engine" PREFIX "Resources" FILES ${RES_FILES})

# compiling options:

target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "ENGINE_EXPORTS")

if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE DEFINITIONS "NOMINMAX") # stop windows.h conflicting with 'std::max'
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /MP)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
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
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC dependencies/stb)

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
set(SPDLOG_SYSTEM_INCLUDES ON CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dependencies/spdlog) # automatically calls target_include_directories
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog)
