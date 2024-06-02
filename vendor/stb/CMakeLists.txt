cmake_minimum_required(VERSION 3.24)

project(stb LANGUAGES C CXX)

set(SRC_FILES
	"src/stb_impl.cpp"
)

set(INCLUDE_FILES
	"include/stb_image.h"
	"include/stb_image_write.h"
	"include/stb_rect_pack.h"
	"include/stb_truetype.h"
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