#pragma once

#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL 2 // info
#else
#define SPDLOG_ACTIVE_LEVEL 0 // trace
#endif

#include <spdlog/spdlog.h>

#define TRACE SPDLOG_TRACE
#define DEBUG SPDLOG_DEBUG
#define INFO SPDLOG_INFO
#define WARN SPDLOG_WARN
#define ERROR SPDLOG_ERROR
#define CRITICAL SPDLOG_CRITICAL