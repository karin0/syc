#include "../ir.hpp"

using namespace ir;

static void dce0(Func *f) {
    // TODO: opt; const prog
    bool changed;
    do {
        changed = false;
        FOR_BB (bb, *f) {
            bool end = false;
            for (auto *i = bb->insts.front; i;) {
                auto *next = i->next;
                if (end || (i->uses.empty() && i->is_pure())) {
                    bb->erase(i);
                    delete i;
                    changed = true;
                } else if (i->is_control())
                    end = true;
                i = next;
            }
        }
    } while (changed);
    info("%s: dce done", f->name.data());
}

static vector<const Use *> get_owned_uses(Inst *i) {
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

static void traverse(Inst *i) {
    i->vis = true;
    for (auto *u: get_owned_uses(i))
        if_a (Inst, x, u->value) if (!x->vis)
            traverse(x);
}

// TODO: keeping dce0 reduces 1 inst for some case
void dce(Func *f) {
    void dbe(Func *);
    dbe(f);

    FOR_BB_INST (i, bb, *f)
        i->vis = false;

    FOR_BB_INST (i, bb, *f)
        if (!i->is_pure())
            traverse(i);

    FOR_BB_INST (i, bb, *f) if (!i->vis) {
        bb->erase_with(i, nullptr);
        delete i;
    }

    dbe(f);  // works anyhow
}
