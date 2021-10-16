#include "common.hpp"

#ifdef SYC_LOG

#include <chrono>
#include <iomanip>
#include <ctime>

char log_sbuf[1024];

void put_log_prompt(const char *level) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const auto lt = std::localtime(&t);

    std::cerr
        << std::put_time(lt, "%F %T.")
        << duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000
        << level;
}

#endif

#ifdef SYC_ASSERTS

void asserts(bool cond) {
    if (!cond)
        throw std::logic_error{"assertion failed"};
}

#endif

const char* __asan_default_options() {
    return "detect_leaks=0";
}
