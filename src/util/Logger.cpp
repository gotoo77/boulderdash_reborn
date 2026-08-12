#include "Logger.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

std::string timestamp() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto time = clock::to_time_t(now);
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

} // namespace

void Logger::setLevel(LogLevel level) {
    s_level = level;
}

void Logger::setEnabled(bool enabled) {
    s_enabled = enabled;
}

void Logger::trace(const std::string& message, const char* functionName) {
    log(LogLevel::Trace, "TRACE", message, functionName);
}

void Logger::debug(const std::string& message, const char* functionName) {
    log(LogLevel::Debug, "DEBUG", message, functionName);
}

void Logger::info(const std::string& message, const char* functionName) {
    log(LogLevel::Info, "INFO", message, functionName);
}

void Logger::warn(const std::string& message, const char* functionName) {
    log(LogLevel::Warn, "WARN", message, functionName);
}

void Logger::error(const std::string& message, const char* functionName) {
    log(LogLevel::Error, "ERROR", message, functionName);
}

void Logger::log(LogLevel level, const char* label, const std::string& message, const char* functionName) {
    if (!s_enabled || level < s_level) {
        return;
    }

    std::lock_guard<std::mutex> guard(s_mutex);
    std::clog << "[" << timestamp() << "] " << label;
    if (functionName && *functionName) {
        std::clog << " [" << functionName << "]";
    }
    std::clog << ": " << message << '\n';
}

void Logger::logf(LogLevel level, const char* label, const char* functionName, const char* fmt, ...) {
    if (!s_enabled || level < s_level) {
        return;
    }

    char buffer[2048];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    log(level, label, buffer, functionName);
}
