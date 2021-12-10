#include "ir_common.hpp"

bool dcbe(Func *);
bool dge(Prog *);
void dle(Func *);
void mem2reg(Func *);
void br_induce(Func *);
void gg(Func *f);

template <class T>
static Prog &operator << (Prog &lh, T (*rh)(Func *)) {
    for (auto &func: lh.funcs)
        rh(&func);
    return lh;
}

template <class T>
static Prog &operator << (Prog &lh, T (*rh)(Prog *)) {
    rh(&lh);
    return lh;
}

static bool any(Prog &lh, bool (*rh)(Func *)) {
    bool res = false;
    for (auto &func: lh.funcs)
        res = rh(&func) || res;
    return res;
}

void cd(Prog *prog) {
    *prog << cg << dcbe;
}

void all(Prog *prog) {
    *prog << cd << gg << dle;
}

void run_passes(Prog &prog, bool opt) {
    if (!opt) {
        prog << cd;  // dcbe is required
        return;
    }
    prog << cd << dge << mem2reg << all << all << cd
         << br_induce
         << build_loop;
}
