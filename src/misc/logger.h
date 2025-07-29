#pragma once
#define ENABLE_LOGGING 1

#if ENABLE_LOGGING || defined(_DEBUG)
#define LOGGING_ENABLED 1
#else
#define LOGGING_ENABLED 0
#endif

#if LOGGING_ENABLED
#define QUILL_DISABLE_NON_PREFIXED_MACROS
#include <quill/Backend.h>
#include <quill/Logger.h>
#include <quill/LogMacros.h>



class ConsoleManager
{
    static bool allocated_;

public:
    static void create_console();
    static void destroy_console();
};

void SetupQuill(char const* logFile);

inline quill::Logger* globalLogger;
inline HWND hConsole;

#define LOG_IMPL(level, fmt, ...) QUILL_LOG_##level(globalLogger, fmt, ##__VA_ARGS__)
#define LOG_IMPL_DYNAMIC(log_level, fmt, ...) QUILL_LOG_DYNAMIC(globalLogger, log_level, fmt, ##__VA_ARGS__)
#else
#define LOG_IMPL(level, fmt, ...) (void)0
#define LOG_IMPL_DYNAMIC(log_level, fmt, ...) (void)0
#endif

#define LOG_TRACE_L3(fmt, ...) LOG_IMPL(TRACE_L3, fmt, ##__VA_ARGS__)
#define LOG_TRACE_L2(fmt, ...) LOG_IMPL(TRACE_L2, fmt, ##__VA_ARGS__)
#define LOG_TRACE_L1(fmt, ...) LOG_IMPL(TRACE_L1, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_IMPL(DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_IMPL(INFO, fmt, ##__VA_ARGS__)
#define LOG_NOTICE(fmt, ...) LOG_IMPL(NOTICE, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG_IMPL(WARNING, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_IMPL(ERROR, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) LOG_IMPL(CRITICAL, fmt, ##__VA_ARGS__)
#define LOG_BACKTRACE(fmt, ...) LOG_IMPL(BACKTRACE, fmt, ##__VA_ARGS__)
#define LOG_DYNAMIC(log_level, fmt, ...) LOG_IMPL_DYNAMIC(log_level, fmt, ##__VA_ARGS__)