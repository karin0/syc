#include "passes.hpp"

using namespace ir;

void dbe(Func *);
void dce(Func *);
void mem2reg(Func *);

void run_passes(Prog &prog) {
    auto run = [&](void (*p)(Func *)) {
        for (auto &func : prog.funcs)
            p(&func);
    };
    run(dbe);  // remove unreachable branch/jumps so that the correct CFG can be produced
    run(dce);
    run(mem2reg);
    run(dce);  // TODO: this is required or things break
}
