#pragma once

#include <Core/Types.hpp>

#include <stdarg.h>
#include <stdio.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log Levels

enum LogLevel : u8 {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warning = 3,
  Error = 4,
  Fatal = 5,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configuration

#if !defined(PIPELINE_LOG_LEVEL)
	#if defined(PIPELINE_DEBUG)
	#define PIPELINE_LOG_LEVEL 0
#else
	#define PIPELINE_LOG_LEVEL 0
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Console Colors

#define LOG_COLOR_TRACE "\x1b[90m"
#define LOG_COLOR_DEBUG "\x1b[36m"
#define LOG_COLOR_INFO "\x1b[32m"
#define LOG_COLOR_WARNING "\x1b[33m"
#define LOG_COLOR_ERROR "\x1b[31m"
#define LOG_COLOR_FATAL "\x1b[41m"
#define LOG_COLOR_RESET "\x1b[0m"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Logger

class Logger
{
public:
	STATIC void Log(LogLevel level, const char *file, i32 line, const char *fmt,
				  ...) {
	const char *levelStr = LevelToString(level);
	const char *levelColor = LevelToColor(level);

	fprintf(stdout, "%s[%s] %s:%d: ", levelColor, levelStr, file, line);

	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);

	fprintf(stdout, "%s\n", LOG_COLOR_RESET);

	if (level == LogLevel::Fatal) {
	  fflush(stdout);
	}
  }

private:
  STATIC const char *LevelToString(LogLevel level) {
	switch (level) {
	case LogLevel::Trace:
	  return "TRACE";
	case LogLevel::Debug:
	  return "DEBUG";
	case LogLevel::Info:
	  return "INFO";
	case LogLevel::Warning:
	  return "WARN";
	case LogLevel::Error:
	  return "ERROR";
	case LogLevel::Fatal:
	  return "FATAL";
	}

	return "UNKNOWN";
  }

  STATIC const char *LevelToColor(LogLevel level) {
	switch (level) {
	case LogLevel::Trace:
	  return LOG_COLOR_TRACE;
	case LogLevel::Debug:
	  return LOG_COLOR_DEBUG;
	case LogLevel::Info:
	  return LOG_COLOR_INFO;
	case LogLevel::Warning:
	  return LOG_COLOR_WARNING;
	case LogLevel::Error:
	  return LOG_COLOR_ERROR;
	case LogLevel::Fatal:
	  return LOG_COLOR_FATAL;
	}

	return LOG_COLOR_RESET;
  }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Logging Macros

#if PIPELINE_LOG_LEVEL <= 0
#define LOG_TRACE(fmt, ...)                                                    \
  Logger::Log(LogLevel::Trace, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...)
#endif

#if PIPELINE_LOG_LEVEL <= 1
#define LOG_DEBUG(fmt, ...)                                                    \
  Logger::Log(LogLevel::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if PIPELINE_LOG_LEVEL <= 2
#define LOG_INFO(fmt, ...)                                                     \
  Logger::Log(LogLevel::Info, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if PIPELINE_LOG_LEVEL <= 3
#define LOG_WARNING(fmt, ...)                                                  \
  Logger::Log(LogLevel::Warning, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_WARNING(fmt, ...)
#endif

#if PIPELINE_LOG_LEVEL <= 4
	#define LOG_ERROR(fmt, ...) Logger::Log(LogLevel::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
	#define LOG_ERROR(fmt, ...)
#endif

#define LOG_FATAL(fmt, ...)  Logger::Log(LogLevel::Fatal, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////