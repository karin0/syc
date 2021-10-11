#include "passes.hpp"

using namespace ir;

void mem2reg(Func *);
void dce(Func *);

static void run(void (*p)(Func *), Prog &prog) {
    for (auto &func : prog.funcs)
        p(&func);
}

void run_passes(Prog &prog) {
    run(mem2reg, prog);
    run(dce, prog);
}
