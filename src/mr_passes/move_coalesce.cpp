#include "../mips.hpp"

using namespace mips;

void move_coalesce(Func *f) {
    FOR_BB (bb, *f) {
        for (auto *i = bb->insts.front; i; ) {
            auto *next = i->next;
            if_a (BinaryInst, x, i) {
                if ((x->op == BinaryInst::Add || x->op == BinaryInst::Sub || x->op == BinaryInst::Xor) &&
                    x->rhs == Operand::make_const(0)) {
                    if (x->dst.equiv(x->lhs))
                        bb->insts.erase(x);
                    else
                        bb->insts.replace(x, new MoveInst{x->dst, x->lhs});
                    delete i;
                }
            } else if_a (MoveInst, x, i) {
                if (x->dst.equiv(x->src)) {
                    bb->insts.erase(x);
                    delete i;
                }
            }
            i = next;
        }
    }
}
