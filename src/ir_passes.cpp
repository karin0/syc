#include "ir_common.hpp"

void dcbe(Func *);
void mem2reg(Func *);
void br_induce(Func *);
void gvn_gcm(Func *f);

static Prog &operator << (Prog &lh, void (*rh)(Func *)) {
    for (auto &func: lh.funcs)
        rh(&func);
    return lh;
}

void run_passes(Prog &prog) {
    prog << dcbe << mem2reg
         << dcbe // TODO: this is required or things break (undef?)
         << gvn_gcm
         << dcbe
         << gvn_gcm
         << dcbe
         << br_induce
         << build_loop;
}
