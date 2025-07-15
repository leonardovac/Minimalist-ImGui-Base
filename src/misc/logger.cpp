#include "logger.h"
#if LOGGING_ENABLED
#include <quill/Frontend.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

// Static member definition
bool ConsoleManager::allocated_ = false;

void ConsoleManager::create_console()
{
    if (allocated_) return;

    if (AllocConsole())
    {
        if (IsValidCodePage(CP_UTF8))
        {
            SetConsoleCP(CP_UTF8);
            SetConsoleOutputCP(CP_UTF8);
        }

        if (const HANDLE hStd = GetStdHandle(STD_OUTPUT_HANDLE); hStd != INVALID_HANDLE_VALUE)
        {
            SetConsoleMode(hStd, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        SetConsoleCtrlHandler(nullptr, TRUE);

        if (freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout) == 0 &&
            freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr) == 0)
        {
            std::ios::sync_with_stdio();
            SetConsoleTitleW(L"Log Console");
			hConsole = GetConsoleWindow();
            allocated_ = true;
        }
    }
}

void ConsoleManager::destroy_console()
{
    if (!allocated_) return;

    FreeConsole();
    allocated_ = false;
}

void SetupQuill(char const* logFile)
{
    quill::Backend::start();

    quill::ConsoleSinkConfig consoleSinkConfig{};
    quill::ConsoleSinkConfig::Colours colours;
    colours.assign_colour_to_log_level(quill::LogLevel::Notice, quill::ConsoleSinkConfig::Colours::blue);
	consoleSinkConfig.set_colours(colours);

    auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink", consoleSinkConfig);
    auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        logFile,
        []
        {
            quill::FileSinkConfig cfg;
            cfg.set_open_mode('a');
            cfg.set_filename_append_option(quill::FilenameAppendOption::StartDate);
            return cfg;
        }(),
        quill::FileEventNotifier{});

    globalLogger = quill::Frontend::create_or_get_logger(
        "global",
        { std::move(consoleSink), std::move(fileSink) },
        quill::PatternFormatterOptions
        {
            "%(time:>8) [%(thread_id:>5)] %(short_source_location:<16) %(log_level:<9) [%(caller_function)] %(message)",
            "%H:%M:%S"
        }
    );

#ifdef _DEBUG
    globalLogger->set_log_level(quill::LogLevel::Debug);
#endif

    quill::Frontend::preallocate();
}
#endif