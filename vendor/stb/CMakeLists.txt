cmake_minimum_required(VERSION 3.24)

project(stb LANGUAGES C)

set(SRC_FILES
	"src/stb_impl.c"
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

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)