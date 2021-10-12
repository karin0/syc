#pragma once

#include <stdexcept>

#ifdef SYC_LOG

#include <vector>
#include <iostream>

void put_log_prompt(const char *level);

extern char log_sbuf[1024];

template <typename... Args>
void log(const char *level, const char *fn, const char *suf, const char *fmt, const Args&... args) {
    put_log_prompt(level);
    int r = std::snprintf(log_sbuf, sizeof log_sbuf, fmt, args...);
    if (r >= 0 && std::size_t(r) < sizeof log_sbuf)
        std::cerr << log_sbuf;
    else {
        std::size_t n = 1 + std::snprintf(nullptr, 0, fmt, args...);
        std::vector<char> buf(n);
        std::snprintf(buf.data(), n, fmt, args...);
        std::cerr << buf.data();
    }
    std::cerr << " (" << fn << suf << std::endl;
}

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

#define FILE_NAME (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#define LOGS(lvl, ...) log(" [" lvl "] ", FILE_NAME, ":" STRINGIZE(__LINE__) ")",

#define error(...) LOGS("ERROR") __VA_ARGS__)
#define warn(...) LOGS("WARN") __VA_ARGS__)
#define info(...) LOGS("INFO") __VA_ARGS__)
#define debug(...) LOGS("DEBUG") __VA_ARGS__)
#define trace(...) LOGS("TRACE") __VA_ARGS__)
#define fatal(...)  do { LOGS("FATAL") __VA_ARGS__); throw std::runtime_error{"fatal error"}; } while (0)

template <typename T>
void log_all(const T &x) {
    std::cerr << x;
}

template <typename T, typename... Args>
void log_all(const T &x, const Args&... args) {
    std::cerr << x << ' ';
    log_all(args...);
}

template <typename... Args>
void logf(const char *level, const char *fn, const char *suf, const Args&... args) {
    put_log_prompt(level);
    log_all(args...);
    std::cerr << " (" << fn << suf << std::endl;
}

#define LOGFS(lvl, ...) logf(" [" lvl "] ", FILE_NAME, ":" STRINGIZE(__LINE__) ")",
#define errorf(...) LOGFS("ERROR") __VA_ARGS__)
#define warnf(...) LOGFS("WARN") __VA_ARGS__)
#define infof(...) LOGFS("INFO") __VA_ARGS__)
#define debugf(...) LOGFS("DEBUG") __VA_ARGS__)
#define tracef(...) LOGFS("TRACE") __VA_ARGS__)

#else

#define error(...)  void(0)
#define warn(...)  void(0)
#define info(...)   void(0)
#define debug(...)  void(0)
#define trace(...)  void(0)
#define fatal(msg, ...) (throw std::runtime_error{(msg)})

#define errorf(...) void(0)
#define warnf(...) void(0)
#define infof(...) void(0)
#define debugf(...) void(0)
#define tracef(...) void(0)

#endif

#ifdef SYC_ASSERTS
void asserts(bool cond);
#else
#define asserts(...) void(0)
#endif

#define unreachable() fatal("unreachable")
