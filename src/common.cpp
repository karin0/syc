#ifdef SYC_LOG

#include "common.hpp"
#include <chrono>
#include <iomanip>

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
