cmake_minimum_required(VERSION 3.24)

project(json LANGUAGES CXX)

set(INCLUDE_FILES
	"include/json.hpp"
)

add_library(${PROJECT_NAME} INTERFACE
	${INCLUDE_FILES}
)