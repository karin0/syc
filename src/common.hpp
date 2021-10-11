#pragma once

#include <ctime>

#include <chrono>
#include <vector>
#include <iomanip>
#include <iostream>

#ifdef SYC_LOG

template <typename... Args>
void log(const char *level, const char *fn, const char *suf, const char *fmt, Args... args) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const auto lt = std::localtime(&t);

    // TODO: use a static buffer
    // TODO: use a stream styled logger
    std::size_t n = 1 + std::snprintf(nullptr, 0, fmt, args...);
    std::vector<char> buf(n);
    std::snprintf(buf.data(), n, fmt, args...);

    std::cerr
        << std::put_time(lt, "%F %T.")
        << duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000
        << level << buf.data() << " (" << fn << suf << std::endl;
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
#define fatal(...)  do { LOGS("FATAL") __VA_ARGS__); throw std::runtime_error("fatal error"); } while (0)
#define asserts(cond) do { if (!(cond)) { fatal("assertion failed: %s", STRINGIZE(cond)); } } while (0)

#else

#define log(...)    void(0)
#define error(...)  void(0)
#define warn(...)  void(0)
#define info(...)   void(0)
#define debug(...)  void(0)
#define trace(...)  void(0)
#define fatal(msg, ...) (throw msg)
#define asserts(...) void(0)

#endif

#define unreachable() fatal("unreachable")
