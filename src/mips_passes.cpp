#include "passes.hpp"

using namespace mips;

void reg_alloc(Func *);

static void run(void (*p)(Func *), Prog &prog) {
    for (auto &func : prog.funcs)
        p(&func);
}

void run_mips_passes(mips::Prog &prog) {
    run(reg_alloc, prog);
}
