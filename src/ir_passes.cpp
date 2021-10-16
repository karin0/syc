#include "passes.hpp"

using namespace ir;

void dce(Func *);
void mem2reg(Func *);
void br_induce(Func *);

static Prog &operator << (Prog &lh, void (*rh)(Func *)) {
    for (auto &func: lh.funcs)
        rh(&func);
    return lh;
}

void run_passes(Prog &prog) {
    prog << dce << mem2reg
         << dce // TODO: this is required or things break (undef?)
         << br_induce;
}
