#include "prompt.hpp"
#include "common.hpp"
#include <algorithm>
#include <utility>
#include <vector>

static std::ostream *outsp;

void set_os(std::ostream &outs) {
    outsp = &outs;
}

std::ostream &get_os() {
    return *outsp;
}

#ifdef SYC_ERROR_PROMPT

static std::vector<std::vector<std::pair<int, int>>> errs = {{}};

void push_err(int ty, int ln) {
    infof(errs.size() - 1, "got err", char(ty), ln);
    errs.back().emplace_back(ln, ty);
}

bool put_errs() {
    auto &errs = ::errs.back();
    if (errs.empty())
        return false;
    std::sort(errs.begin(), errs.end());
    for (auto &p: errs)
        *outsp << p.first << ' ' << char(p.second) << '\n';
    return true;
}

void push_err_mask() {
    errs.emplace_back();
}

void pop_err_mask_resolve() {
    auto &a = errs[errs.size() - 2];
    for (auto &x: errs.back())
        a.push_back(x);
    errs.pop_back();
}

void pop_err_mask_reject() {
    errs.pop_back();
}

#endif
