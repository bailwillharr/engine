cmake_minimum_required(VERSION 3.24)

project(imgui LANGUAGES CXX)

set(SRC_FILES
	"src/imgui.cpp"
	"src/imgui_demo.cpp"
	"src/imgui_draw.cpp"
	"src/imgui_impl_sdl2.cpp"
	"src/imgui_impl_vulkan.cpp"
	"src/imgui_internal.h"
	"src/imgui_tables.cpp"
	"src/imgui_widgets.cpp"
	"src/imstb_textedit.h"
)

set(INCLUDE_FILES
	"include/imconfig.h"
	"include/imgui.h"
	"include/imgui_impl_sdl2.h"
	"include/imgui_impl_vulkan.h"
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

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
target_link_libraries(${PROJECT_NAME} PRIVATE stb)
target_link_libraries(${PROJECT_NAME} PRIVATE volk::volk)