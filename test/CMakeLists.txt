cmake_minimum_required(VERSION 3.12)

# options

project(enginetest LANGUAGES CXX
	VERSION "0.1.0"
)

set(GAME_SOURCES
	"src/camera_controller.cpp"
	"src/camera_controller.hpp"
  "src/game.cpp"
  "src/game.hpp"
  "src/main.cpp"
  "src/meshgen.cpp"
  "src/meshgen.hpp"
)

file(GLOB_RECURSE GAME_RESOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/res/*")

if (WIN32)
	option(ENGINETEST_BUILD_WIN32_APP "Build as a standalone win32 app (no console window)" ON)
endif()

if (WIN32 AND ENGINETEST_BUILD_WIN32_APP)
	add_executable(${PROJECT_NAME} WIN32 ${GAME_SOURCES} ${GAME_RESOURCES} "game.rc")
else()
	add_executable(${PROJECT_NAME} ${GAME_SOURCES} ${GAME_RESOURCES})
endif()

# vs source view
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/res" PREFIX "Resources" FILES ${GAME_RESOURCES})

# compiling options:

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /MP)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)

# Pass some project information into the source code
configure_file(config.h.in config.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE engine)
target_include_directories(${PROJECT_NAME} PRIVATE ../include)

add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${PROJECT_SOURCE_DIR}/res $<TARGET_FILE_DIR:enginetest>/res)

get_target_property(ENGINE_SOURCE_DIR engine SOURCE_DIR)	
add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${ENGINE_SOURCE_DIR}/res/engine $<TARGET_FILE_DIR:enginetest>/res/engine)