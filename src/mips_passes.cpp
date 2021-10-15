#include "passes.hpp"

using namespace mips;

void reg_alloc(Func *);
void reg_restore(Func *);
void move_coalesce(Func *f);

static Prog &operator << (Prog &lh, void (*rh)(Func *)) {
    for (auto &func: lh.funcs)
        rh(&func);
    return lh;
}

void run_mips_passes(Prog &prog) {
    Regs::init();

    prog << move_coalesce  // must preserve arg_loads & allocas
         << reg_alloc << move_coalesce << reg_restore;

    // TODO: movz, movn; madd; reduce syscall lis; alloc sp (or just use sp in load/store)
}
