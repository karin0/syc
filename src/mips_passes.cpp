#include "passes.hpp"

using namespace mips;

void reg_alloc(Func *);
void reg_restore(Func *);
void move_coalesce(Func *f);

void run_mips_passes(mips::Prog &prog) {
    auto run = [&](void (*p)(Func *)) {
        for (auto &f: prog.funcs)
            p(&f);
    };

    Regs::init();
    run(move_coalesce);  // preserve arg_loads & allocas
    run(reg_alloc);
    run(move_coalesce);
    run(reg_restore);

    // TODO: movz, movn; madd; reduce syscall lis; alloc sp
}
