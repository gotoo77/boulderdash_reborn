#pragma once

#include <mutex>
#include <string>
#include <cstdarg>

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static void setEnabled(bool enabled);

    static void trace(const std::string& message, const char* functionName = nullptr);
    static void debug(const std::string& message, const char* functionName = nullptr);
    static void info(const std::string& message, const char* functionName = nullptr);
    static void warn(const std::string& message, const char* functionName = nullptr);
    static void error(const std::string& message, const char* functionName = nullptr);
    static void logf(LogLevel level, const char* label, const char* functionName, const char* fmt, ...);

private:
    static void log(LogLevel level, const char* label, const std::string& message, const char* functionName);

    static inline LogLevel s_level = LogLevel::Info;
    static inline bool s_enabled = true;
    static inline std::mutex s_mutex;
};


#define LOG_T(...) ::Logger::logf(LogLevel::Trace, "TRACE", __func__, __VA_ARGS__)
#define LOG_D(...) ::Logger::logf(LogLevel::Debug, "DEBUG", __func__, __VA_ARGS__)
#define LOG_I(...) ::Logger::logf(LogLevel::Info,  "INFO",  __func__, __VA_ARGS__)
#define LOG_W(...) ::Logger::logf(LogLevel::Warn,  "WARN",  __func__, __VA_ARGS__)
#define LOG_E(...) ::Logger::logf(LogLevel::Error, "ERROR", __func__, __VA_ARGS__)
