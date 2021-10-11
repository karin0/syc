#include "passes.hpp"

using namespace ir;

void mem2reg(Func *);
void dce(Func *);

void run_passes(Prog &prog) {
    auto run = [&](void (*p)(Func *)) {
        for (auto &func : prog.funcs)
            p(&func);
    };
    run(dce);  // must do this first to remove unreachable branch/jumps so that the correct CFG can be produced
    run(mem2reg);
    run(dce);
}
