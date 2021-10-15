#include "ir_common.hpp"
#include <unordered_set>

static void traverse(BB *u) {
    if (u->vis)
        return;
    u->vis = true;
    for (auto v : u->get_succ())
        traverse(v);
}

// TODO: new impl affects A-21, A-1
static Value *simplify_bin(BinaryInst *x) {
    if_a (Const, lc, x->lhs.value)
        if_a (Const, rc, x->rhs.value)
            return Const::of(eval_bin(x->op, lc->val, rc->val));
    if (x->op == tkd::Lt || x->op == tkd::Gt) {
        auto *l = Const::of(Const::MIN), *r = Const::of(Const::MAX);
        if (x->op == tkd::Lt)
            std::swap(l, r);
        if (x->lhs.value == l || x->rhs.value == r)
            return &Const::ZERO;
    } else if (x->op == tkd::Le || x->op == tkd::Ge) {
        auto *l = Const::of(Const::MIN), *r = Const::of(Const::MAX);
        if (x->op == tkd::Ge)
            std::swap(l, r);
        if (x->lhs.value == l || x->rhs.value == r)
            return &Const::ONE;
    }
    // TODO: more rules
    return nullptr;
}

// This ensures BB::get_succ works properly and all bbs are reachable
void dbe(Func *f) {
    FOR_BB (bb, *f) {
        bool end = false;
        FOR_LIST_MUT (i, bb->insts) {
            if (end) {
                bb->erase(i);
                delete i;
            } else if (i->is_control())
                end = true;
        }
    }

    FOR_BB (bb, *f) FOR_LIST_MUT (i, bb->insts) if_a (BinaryInst, x, i) {
        // TODO: x < x+1
        auto *v = simplify_bin(x);
        if (v) {
            bb->erase_with(x, v);
            delete x;
        }
    }

    // TODO: do const prop before this
    FOR_BB (bb, *f) {
        if_a (BranchInst, i, bb->get_control()) {
            if_a (Const, c, i->cond.value) {
                infof("replace br with const cond", c->val);
                auto *j = new JumpInst{c->val ? i->bb_then : i->bb_else};
                j->bb = bb;
                bb->insts.replace(i, j);
                delete i;
            }
        }
    }

    FOR_BB (u, *f)
        u->vis = false;

    traverse(f->bbs.front);
    std::unordered_set<BB *> deleted;
    FOR_LIST_MUT (u, f->bbs) {
        if (!u->vis) {
            infof("unreachable bb", u->id);
            // bb can be referred in some PhiInst!
            for (auto *v: u->get_succ()) if (!deleted.count(v)) {
                FOR_INST (i, *v) {
                    if_a (PhiInst, x, i) {
                        for (auto it = x->vals.begin(); it != x->vals.end(); ++it) {
                            if (it->second == u) {
                                x->vals.erase(it);
                                break;
                            }
                        }
                    } else
                        break;
                }
            }

            FOR_LIST_MUT (i, u->insts) {
                // i->replace_uses(nullptr);
                u->erase_with(i, nullptr);
                delete i;
            }
            f->bbs.erase(u);
            deleted.insert(u);
            delete u;
        }
    }
    // TODO: trivial phi induce

    infof(f->name, ": dbe done");
}
