#pragma once
/**
 * Simple logging framework for the OpenChamp Gameserver.
 * 
 * Usage:
 *   LOG_INFO("Server started on port %d", port);
 *   LOG_ERROR("Failed to initialize: %s", error_msg);
 *   LOG_DEBUG("Player count: %zu", players.size());
 */
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <chrono>
#include <mutex>

enum class LogLevel {
    DEBUG_LEVEL = 0,
    INFO_LEVEL = 1,
    WARN_LEVEL = 2,
    ERROR_LEVEL = 3,
};

class Logger {
public:
    /**
     * Initialize the logger with a minimum log level.
     * Messages below this level will not be printed.
     * @param min_level Minimum level to log (default: INFO_LEVEL)
     */
    static void initialize(LogLevel min_level = LogLevel::INFO_LEVEL) {
        min_log_level = min_level;
    }
    
    /**
     * Set the minimum log level.
     * @param level New minimum level
     */
    static void set_level(LogLevel level) {
        min_log_level = level;
    }
    
    /**
     * Log a message at DEBUG level.
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    static void debug(const char* format, ...) {
        if (min_log_level > LogLevel::DEBUG_LEVEL) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        va_list args;
        va_start(args, format);
        log_impl("DEBUG", format, args);
        va_end(args);
    }
    
    /**
     * Log a message at INFO level.
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    static void info(const char* format, ...) {
        if (min_log_level > LogLevel::INFO_LEVEL) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        va_list args;
        va_start(args, format);
        log_impl("INFO", format, args);
        va_end(args);
    }
    
    /**
     * Log a message at WARN level.
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    static void warn(const char* format, ...) {
        if (min_log_level > LogLevel::WARN_LEVEL) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        va_list args;
        va_start(args, format);
        log_impl("WARN", format, args);
        va_end(args);
    }
    
    /**
     * Log a message at ERROR level.
     * @param format Printf-style format string
     * @param ... Variable arguments
     */
    static void error(const char* format, ...) {
        if (min_log_level > LogLevel::ERROR_LEVEL) return;
        
        std::lock_guard<std::mutex> lock(log_mutex);
        va_list args;
        va_start(args, format);
        log_impl("ERROR", format, args);
        va_end(args);
    }

private:
    static LogLevel min_log_level;
    static std::mutex log_mutex;
    
    static void log_impl(const char* level, const char* format, va_list args) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        char time_buf[32];
#ifdef _WIN32
        struct tm time_info;
        localtime_s(&time_info, &time);
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &time_info);
#else
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&time));
#endif
        
        FILE* output_stream = (strcmp(level, "ERROR") == 0 || strcmp(level, "WARN") == 0) 
            ? stderr : stdout;
        
        fprintf(output_stream, "[%s.%03lld] [%-5s] ", 
               time_buf, (long long)ms.count(), level);
        vfprintf(output_stream, format, args);
        fprintf(output_stream, "\n");
        fflush(output_stream);
    }
};

// Initialize static members
inline LogLevel Logger::min_log_level = LogLevel::INFO_LEVEL;
inline std::mutex Logger::log_mutex;

// Convenience Macros (Readability)
#define LOG_DEBUG(fmt, ...) Logger::debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) Logger::info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) Logger::warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::error(fmt, ##__VA_ARGS__)
