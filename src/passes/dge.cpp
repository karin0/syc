#include "ir_common.hpp"
#include <unordered_map>
#include <unordered_set>

struct S {
    Func *user = nullptr;
    bool stored = false;
};

// Require d is global
// Require mem2reg later
bool try_elim(Decl *d) {
    if (!d->dims.empty())
        return false;

    Global *g = as_a<Global>(d->value);
    asserts(g);

    Func *user = nullptr;
    FOR_LIST (u, g->uses) {
        auto *f = u->user->bb->func;
        if (!user)
            user = f;
        else if (user != f)
            return false;
    }

    if (user) {
        BB *bb = user->bbs.front;
        if (!user->is_once) {
            return false;
            /*
            auto *var = new AllocaInst{d};
            g->replace_uses(var);
            auto *i = new LoadInst{d, g, &Const::ZERO};
            auto *k = new StoreInst{d, var, &Const::ZERO, i};
            bb->push_front(k);
            bb->push_front(var);
            bb->push_front(i);

            FOR_BB_INST (i, bb, *user) {
                if_a (ReturnInst, x, i) {
                    auto *j = new LoadInst{d, var, &Const::ZERO};
                    auto *k = new StoreInst{d, g, &Const::ZERO, j};
                    j->bb = k->bb = bb;
                    bb->insts.insert(i, j);
                    bb->insts.insert(i, k);
                }
            }
             */
        } else {
            auto *i = new AllocaInst{d};
            int v = d->init.empty() ? 0 : d->init.front()->eval();
            auto *j = new StoreInst{d, i, &Const::ZERO, Const::of(v)};
            bb->push_front(j);
            bb->push_front(i);
            g->replace_uses(i);
        }
        infof("dge: move", d->name, "into", user->name);
    } else
        infof("dge: elim", d->name, "with no user");

    return true;
}

void build_once(Prog *p) {
    Func *main = nullptr;
    for (auto &f: p->funcs) {
        if ((f.is_once = !main && f.name == "main"))
            main = &f;
        f.callers.clear();

        FOR_BB (bb, f)
            bb->is_once = false;

        build_pred(&f);
        auto *ent = f.bbs.front;
        ent->is_once = ent->pred.empty();
        if (!ent->is_once)
            continue;
        vector<BB *> wl = {ent};
        while (!wl.empty()) {
            vector<BB *> nl;
            for (auto *u: wl)
                for (auto *v: u->get_succ()) {
                    if (!v->is_once) {
                        bool ok = true;
                        for (auto *p: v->pred)
                            if (!p->is_once) {
                                ok = false;
                                break;
                            }
                        if (ok) {
                            v->is_once = true;
                            nl.push_back(v);
                        }
                    }
                }
            wl = std::move(nl);
        }
    }

    static std::unordered_map<Func *, CallInst *> uniq_call;
    uniq_call.clear();
    for (auto &f: p->funcs)
        uniq_call[&f] = nullptr;

    for (auto &f: p->funcs) {
        FOR_BB_INST (i, bb, f) {
            if_a (CallInst, x, i) {
                auto *g = x->func;
                auto it = uniq_call.find(g);
                if (it != uniq_call.end()) {
                    if (it->second == nullptr)
                        it->second = x;
                    else
                        uniq_call.erase(it);
                }
            }

        }
    }

    for (auto p: uniq_call) {
        if (p.second) {
            auto *bb = p.second->bb;
            if (bb->is_once)
                bb->func->callers.insert(p.first);
        } else if (!p.first->is_once)
            p.first->is_unused = true;  // TODO: drop them
    }

    vector<Func *> wl = {main};
    while (!wl.empty()) {
        vector<Func *> nl;
        for (Func *f: wl) {
            infof("dge: once func", f->name);
            for (Func *g: f->callers)
                if (!g->is_once) {
                    g->is_once = true;
                    nl.push_back(g);
                }
        }
        wl = std::move(nl);
    }
}

// Requires cg?
bool dge(Prog *p) {
    build_once(p);
    bool res = false;
    for (auto *d: p->globals) {
        res = try_elim(d) || res;
    }
    return res;
}
