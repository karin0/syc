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

void run_mips_passes(Prog &prog) {
    Regs::init();

    prog << bb_normalize
         << move_coalesce  // must preserve arg_loads & allocas
         // << dce  // slows down A-13 but speeds A-27
         << reg_alloc << dce << move_coalesce << dce << reg_restore;

    // TODO: movz, movn; madd; reduce syscall lis; alloc sp (or just use sp in load/store)
}
