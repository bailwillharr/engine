#ifndef ENGINE_INCLUDE_LOG_H_
#define ENGINE_INCLUDE_LOG_H_

#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL 2  // info
#else
#define SPDLOG_ACTIVE_LEVEL 0  // trace
#endif

#include <spdlog/spdlog.h>

#define LOG_TRACE SPDLOG_TRACE
#define LOG_DEBUG SPDLOG_DEBUG
#define LOG_INFO SPDLOG_INFO
#define LOG_WARN SPDLOG_WARN
#define LOG_ERROR SPDLOG_ERROR
#define LOG_CRITICAL SPDLOG_CRITICAL

#endif