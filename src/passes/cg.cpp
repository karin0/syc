#include "ir_common.hpp"

// TODO: affects A-13, A-14 even when doing nothing
void build_cg(Prog *p) {
    for (auto &f: p->funcs) {
        f.has_side_effects = false;
        f.callers.clear();
    }
    for (auto &f: p->funcs) {
        FOR_BB_INST (i, bb, f) {
            if_a (CallInst, x, i) {
                x->func->callers.insert(&f);
                if (x->func->has_side_effects)
                    f.has_side_effects = true;
            } else if_a (StoreInst, x, i) {
                if (!f.has_side_effects && (x->lhs->is_global ||
                    (!x->lhs->dims.empty() && x->lhs->dims.front() < 0)
                    ))
                    f.has_side_effects = true;
            }
        }
    }
    vector<Func *> wl;
    for (auto &f: p->funcs) if (f.has_side_effects)
        wl.push_back(&f);
    while (!wl.empty()) {
        auto *u = wl.back();
        wl.pop_back();
        if (u->has_side_effects)
            for (auto *v: u->callers) if (!v->has_side_effects) {
                v->has_side_effects = true;
                wl.push_back(v);
            }
    }
}
