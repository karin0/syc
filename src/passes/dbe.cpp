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
        auto *i = bb->get_control();
        if_a (BranchInst, x, i) {
            if_a (Const, c, x->cond.value) {
                infof("found const cond", c->val);
                auto *j = new JumpInst{c->val ? x->bb_then : x->bb_else};
                j->bb = bb;
                bb->insts.replace(x, j);
                delete x;
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
                    if_a (PhiInst, x, i)
                        vec_erase_if(x->vals, [u](const std::pair<Use, BB *> &p) {
                           return p.second == u;
                        });
                    else
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

    infof(f->name, ": dbe done");
}
