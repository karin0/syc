#include "ir_common.hpp"

void build_pred(Func *f) {
    FOR_BB (u, *f)
        u->pred.clear();
    FOR_BB (u, *f)
        for (BB *v : u->get_succ())
            v->pred.push_back(u);
}

static void traverse(BB *u, BB *block) {
    if (u->vis || u == block)
        return;
    u->vis = true;
    for (auto v : u->get_succ())
        traverse(v, block);
}

static void set_depth(BB *u, int depth) {
    u->dom_depth = depth;
    for (auto *p: u->dom_chs)
        set_depth(p, depth + 1);
}

void build_dom(Func *f) {
    FOR_BB (u, *f) {
        u->dom.clear();
        u->dom_chs.clear();
        u->idom = nullptr;
    }

    FOR_BB (w, *f) {
        FOR_BB (u, *f)
            u->vis = false;

        traverse(f->bbs.front, w);
        FOR_BB (u, *f) if (!u->vis) {
                u->dom.insert(w);
                info("%s: bb_%d doms bb_%d", f->name.data(), w->id, u->id);
            }
    }

    FOR_BB (u, *f) {
        for (BB *w : u->dom) if (u != w) {
                bool ok = true;
                for (BB *v : u->dom)
                    if (v != u && w != v && v->dom.count(w)) {
                        ok = false;
                        break;
                    }
                if (ok) {
                    u->idom = w;
                    w->dom_chs.insert(u);
                    info("%s: bb_%d idoms bb_%d", f->name.data(), w->id, u->id);
                    break;
                }
            }
    }

    set_depth(f->bbs.front, 0);
}

vector<const Use *> get_owned_uses(Inst *i) {
    if_a (BinaryInst, x, i)
        return {&x->lhs, &x->rhs};
    else if_a (CallInst, x, i) {
        vector<const Use *> res;
        for (auto &u: x->args)
            res.push_back(&u);
        return res;
    } else if_a (BranchInst, x, i)
        return {&x->cond};
    else if_a (ReturnInst, x, i) {
        if (x->val.value)
            return {&x->val};
    } else if_a (LoadInst, x, i)
        return {&x->base, &x->off};
    else if_a (StoreInst, x, i)
        return {&x->val, &x->base, &x->off};
    else if_a (GEPInst, x, i)
        return {&x->base, &x->off};
    else if_a (PhiInst, x, i) {
        vector<const Use *> res;
        for (auto &u: x->vals)
            res.push_back(&u.first);
        return res;
    }
    return {};
}
