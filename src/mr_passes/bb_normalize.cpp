#include "../mips.hpp"

using namespace mips;

// normalize every bb to [other..] [br..] [jump/return] by splitting
// (expanding phi nodes breaks this)
// TODO: this affects A-2, A-14 (significantly), B-2, B-26
void bb_normalize(Func *f) {
    for (auto *bb = f->bbs.front; bb; bb = bb->next) {
        bool branched = false;
        FOR_INST (i, *bb) {
            if (is_a<BaseBranchInst>(i))
                branched = true;
            else if (is_a<JumpInst>(i) || is_a<ReturnInst>(i)) {
                // delete all after insts
                for (Inst *j = i->next, *j_next; j; j = j_next) {
                    j_next = j->next;
                    bb->insts.erase(j);
                    delete j;
                }
                break;
            } else if (branched) {
                // move i and all after insts into a new bb
                auto *nbb = f->new_bb_after(bb);
                nbb->loop_depth = bb->loop_depth;
                for (Inst *j = i, *j_next; j; j = j_next) {
                    j_next = j->next;
                    bb->insts.erase(j);
                    nbb->insts.push(j);
                }
                break;
            }
        }
    }

    // TODO: this method may push duplicated bbs
    FOR_LIST_MUT (bb, f->bbs) {
        bool fall = true;
        FOR_INST (i, *bb) {
            if (is_a<ReturnInst>(i)) {
                asserts(i->next == nullptr);
                fall = false;
                break;
            } else if_a (ControlInst, x, i) {
                bb->succ.push_back(x->to);
                if (is_a<JumpInst>(x)) {
                    asserts(i->next == nullptr);
                    fall = false;
                    break;
                }
            }
        }
        if (fall) {
            asserts(bb->next);
            bb->succ.push_back(bb->next);
        }
    }
}
