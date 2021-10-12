#include "use_def.hpp"

using namespace mips;

void reg_restore(Func *f) {
    std::set<uint> s_regs;  // s*, ra
    if (!f->is_main) {
        bool is_leaf = true;
        FOR_MBB_MINST (i, bb, *f) {
            if (is_a<CallInst>(i))
                is_leaf = false;
            for (auto x: get_def(i))
                if (Regs::is_s(x))
                    s_regs.insert(x.val);
        }
        if (!is_leaf)
            s_regs.insert(Regs::ra);
    }

    uint stack_size = (f->max_call_arg_num + f->alloca_num + f->spill_num + s_regs.size()) << 2;
    std::fprintf(stderr, "%s has stack size %u (%u args, %u allocas, %u spills, %zu s-regs)\n",
         f->ir->name.data(), stack_size, f->max_call_arg_num, f->alloca_num, f->spill_num, s_regs.size());

    for (auto *i: f->arg_loads)
        i->off = int(stack_size + ((i->off - MAX_ARG_REGS) << 2));  // move_coal won't break this

    if (!stack_size)
        return;

    auto *push = new BinaryInst{BinaryInst::Add,
        Reg::make_machine(Regs::sp), Reg::make_machine(Regs::sp), Operand::make_const(-int(stack_size))
    };
    auto *bb_start = f->bbs.front;
    bb_start->insts.push_front(push);

    int base = int(f->max_call_arg_num + f->alloca_num + f->spill_num) << 2;
    int p = base;
    Inst *inst = push;
    for (auto id: s_regs) {
        inst = bb_start->insts.insert_after(inst, new StoreInst{
            Reg::make_machine(id), Reg::make_machine(Regs::sp), p
        });
        p += 4;
    }

    FOR_MBB_MINST (i, bb, *f) {
        if_a (ReturnInst, x, i) {
            p = base;
            for (auto id: s_regs) {
                bb->insts.insert(x, new LoadInst{
                    Reg::make_machine(id), Reg::make_machine(Regs::sp), p
                });
                p += 4;
            }
            if (!f->is_main)
                bb->insts.insert(x, new BinaryInst{BinaryInst::Add,
                   Reg::make_machine(Regs::sp), Reg::make_machine(Regs::sp), Operand::make_const(int(stack_size))
                });
        }
    }
}
