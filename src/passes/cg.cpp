#include "ir_common.hpp"

// TODO: affects A-13, A-14 even when doing nothing
void cg(Prog *p) {
    for (auto &f: p->funcs) {
        f.callers.clear();
        f.used_callers.clear();
        f.has_side_effects = false;
        f.has_global_loads = false;
        f.has_param_loads = false;
    }
    for (auto &f: p->funcs) {
        FOR_BB_INST (i, bb, f) {
            if_a (CallInst, x, i) {
                x->func->callers.insert(&f);
                if (x->func->has_side_effects)
                    f.has_side_effects = true;
                if (!x->uses.empty())
                    x->func->used_callers.insert(&f);
            } else if_a (StoreInst, x, i) {
                if (!f.has_side_effects && (x->lhs->is_global ||
                    (!x->lhs->dims.empty() && x->lhs->dims.front() < 0)
                    ))
                    f.has_side_effects = true;
            } else if_a (LoadInst, x, i) {
                if (x->lhs->is_global) {
                    if (!x->lhs->is_const)
                        f.has_global_loads = true;
                } else if_a (Argument, a, x->base.value) {
                    asserts(!a->var->dims.empty() && a->var->dims[0] == -1);
                    f.has_param_loads = true;
                }
            } else if_a (GEPInst, x, i) {
                if_a (Argument, a, x->base.value) {
                    asserts(!a->var->dims.empty() && a->var->dims[0] == -1);
                    f.has_param_loads = true;
                }
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

    wl.clear();
    for (auto &f: p->funcs) if (f.has_global_loads)
        wl.push_back(&f);
    while (!wl.empty()) {
        auto *u = wl.back();
        wl.pop_back();
        if (u->has_global_loads)
            for (auto *v: u->used_callers) if (!v->has_global_loads) {
                v->has_global_loads = true;
                wl.push_back(v);
            }
    }

    for (auto &f: p->funcs) {
        f.is_pure = !(f.has_side_effects || f.has_global_loads || f.has_param_loads);
        if (f.is_pure)
            infof(f.name, "is pure");
    }
}
