#include "passes.hpp"

using namespace mips;

void reg_alloc(Func *);
void reg_restore(Func *);
void move_coalesce(Func *f);

void run_mips_passes(mips::Prog &prog) {

#define RUN(p) do \
    for (auto &f: prog.funcs) \
        (p)(&f);  \
    while (0)

    Regs::init();
    RUN(move_coalesce);  // must preserve arg_loads & allocas
    RUN(reg_alloc);
    RUN(move_coalesce);
    RUN(reg_restore);

    // TODO: movz, movn; madd; reduce syscall lis; alloc sp (or just use sp in load/store)
}
