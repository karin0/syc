#include "passes.hpp"

using namespace ir;

void dbe(Func *);
void dce(Func *);
void mem2reg(Func *);
void br_induce(Func *);

void run_passes(Prog &prog) {
    auto run = [&](void (*p)(Func *)) {
        for (auto &func : prog.funcs)
            p(&func);
    };
    run(dbe);  // remove unreachable branch/jumps so that the correct CFG can be produced
    run(dce);
    run(dbe);
    run(mem2reg);
    run(dce);  // TODO: this is required or things break
    run(dbe);  // TODO: this is required or things break (undef?)
    run(br_induce);
}
