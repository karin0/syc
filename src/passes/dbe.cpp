#include "../ir.hpp"

using namespace ir;

static void traverse(BB *u) {
    if (u->vis)
        return;
    u->vis = true;
    for (auto v : u->get_succ())
        traverse(v);
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

    // TODO: do const prop before this
    FOR_BB (bb, *f) {
        if_a (BranchInst, i, bb->get_control()) {
            BB *to = nullptr;
            if_a (Const, c, i->cond.value) {
                infof("found const cond", c->val);
                to = c->val ? i->bb_then : i->bb_else;
            } else if_a (BinaryInst, x, i->cond.value) {
                // TODO: x < x+1
                if_a (Const, lc, x->lhs.value) {
                    if_a (Const, rc, x->rhs.value) {
                        to = eval_bin(x->op, lc->val, rc->val) ? i->bb_then : i->bb_else;
                        goto qwq;
                    }
                }
                if (x->op == tkd::Lt || x->op == tkd::Gt) {
                    auto *l = Const::of(Const::MIN), *r = Const::of(Const::MAX);
                    if (x->op == tkd::Lt)
                        std::swap(l, r);
                    if (x->lhs.value == l || x->rhs.value == r)
                        to = i->bb_else;
                } else if (x->op == tkd::Le || x->op == tkd::Ge) {
                    auto *l = Const::of(Const::MIN), *r = Const::of(Const::MAX);
                    if (x->op == tkd::Ge)
                        std::swap(l, r);
                    if (x->lhs.value == l || x->rhs.value == r)
                        to = i->bb_then;
                }
            }
            qwq:
            if (to) {
                info("found const br");
                auto *j = new JumpInst{to};
                j->bb = bb;
                bb->insts.replace(i, j);
                delete i;
            }
        }
    }

    FOR_BB (u, *f)
        u->vis = false;

    traverse(f->bbs.front);
    FOR_LIST_MUT (u, f->bbs) {
        if (!u->vis) {
            infof("unreachable bb", u->id);
            // bb can be referred in some PhiInst!
            for (auto *v: u->get_succ()) {
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
                i->replace_uses(nullptr);
                delete i;
            }
            f->bbs.erase(u);
            delete u;
        }
    }
    // TODO: trivial phi induce

    infof(f->name, ": dbe done");
}
