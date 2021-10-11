#include "prompt.hpp"

static std::ostream *outsp;

void set_os(std::ostream &outs) {
    outsp = &outs;
}

std::ostream &get_os() {
    return *outsp;
}
