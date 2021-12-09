#include "passes.hpp"

using namespace mips;

void bb_normalize(Func *f);
void reg_alloc(Func *);
void reg_restore(Func *);
void move_coalesce(Func *f);
void dce(Func *f);

static Prog &operator << (Prog &lh, void (*rh)(Func *)) {
    for (auto &func: lh.funcs)
        rh(&func);
    return lh;
}

void run_mips_passes(Prog &prog, bool opt) {
    // TODO: non-opt
    Regs::init();

    prog << bb_normalize
         // << dce  // slows down A-13 but speeds A-2
         << move_coalesce  // must preserve arg_loads & allocas7
         << reg_alloc << dce << move_coalesce << dce << reg_restore;
}
