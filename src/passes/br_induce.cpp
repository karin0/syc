#include "../ir.hpp"

using namespace ir;

// This should be the last pass before building mr to induce BinaryBranchInst
void br_induce(Func *f) {
    FOR_BB (bb, *f) {
        if_a (BranchInst, x, bb->get_control()) {
            if_a (BinaryInst, cond, x->cond.value) {
                if (cond->uses.front == cond->uses.back) {
                    asserts(cond->uses.front == &x->cond);
                    RelOp op;
                    switch (cond->op) {
                        case tkd::Eq: op = RelOp::Eq; break;
                        case tkd::Ne: op = RelOp::Ne; break;
                        case tkd::Lt: op = RelOp::Lt; break;
                        case tkd::Le: op = RelOp::Le; break;
                        case tkd::Gt: op = RelOp::Gt; break;
                        case tkd::Ge: op = RelOp::Ge; break;
                        default:
                            continue;
                    }
                    x->cond.release();
                    infof(f->name, bb->id, "br ind", x, cond);
                    auto *n = new BinaryBranchInst{op, cond, x};
                    cond->bb->erase(cond);  // TODO: if cond is not prev of x, this may expand its live range
                    delete cond;
                    n->bb = bb;
                    bb->insts.replace(x, n);
                    delete x;
                }
            }
        }
    }
}
