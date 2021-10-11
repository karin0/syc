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
    // run(move_coalesce);
    run(reg_alloc);
    run(reg_restore);
    run(move_coalesce);  // TODO: this breaks insts to fix later ... but we need it before alloc to turn gep adds to moves

    // TODO: movz, movn; reduce syscall lis; alloc sp
}
