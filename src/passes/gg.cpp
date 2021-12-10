#include "ir_common.hpp"

static Value *reduced_bin(BinaryInst *x) {
    if_a (Const, lc, x->lhs.value) {
        if_a (Const, rc, x->rhs.value)
            return Const::of(eval_bin(x->op, lc->val, rc->val));
    } else if (!is_a<Const>(x->rhs.value))
        return nullptr;

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
    } else if (x->op == tkd::Add) {
        if (x->lhs.value == &Const::ZERO)
            return x->rhs.value;
        if (x->rhs.value == &Const::ZERO)
            return x->lhs.value;
    } else if (x->op == tkd::Sub) {
        if (x->rhs.value == &Const::ZERO)
            return x->lhs.value;
    } else if (x->op == tkd::Mul) {
        if (x->lhs.value == &Const::ZERO || x->rhs.value == &Const::ZERO)
            return &Const::ZERO;
        if (x->lhs.value == &Const::ONE)
            return x->rhs.value;
        if (x->rhs.value == &Const::ONE)
            return x->lhs.value;
    } else if (x->op == tkd::Div) {
        if (x->rhs.value == &Const::ONE)
            return x->lhs.value;
    } else if (x->op == tkd::Mod) {
        if (x->rhs.value == &Const::ONE)
            return &Const::ZERO;
    }
    return nullptr;
}

static void build_po(BB *u, vector<BB *> &res) {
    if (u->vis)
        return;
    u->vis = true;
    for (auto *v: u->get_succ())
        build_po(v, res);
    res.push_back(u);
}

struct GVN {
    vector<std::pair<Value *, Value *>> vn;

    template<class T>
    Value *find_a(T *) {
        unreachable();
    }

    struct LazyValue {
        Value *const v, *res = nullptr;
        GVN *const ctx;

        LazyValue(Value *v, GVN *ctx) : v(v), ctx(ctx) {}

        operator Value * () {
            if (!res)
                res = ctx->get(v);
            return res;
        }
    };

    LazyValue lazy_get(Value *v) {
        return {v, this};
    }

    template <class T>
    Value *get_a(T *i) {
        for (auto &p: vn) if (p.first == i)
            return p.second;
        auto *v = find_a<T>(i);
        vn.emplace_back(i, v);
        return v;
    }

    Value *get(Value *i);

    void replace(Inst *i, Value *v) {
        if (i != v) {
            i->bb->erase_with(i, v);
            for (auto &p: vn) if (p.first == i) { // TODO
                std::swap(p, vn.back());
                vn.pop_back();
                break;
            }
            delete i;
        }
    }

    template <class T>
    void replace(T *i) {
        replace(i, get_a(i));
    }

    void check(Inst *i) {
        if_a (BinaryInst, x, i) {
            auto *v = reduced_bin(x);
            if (v)
                replace(i, get(v));  // TODO: no get
            else
                replace(x);
        } else if_a (CallInst, x, i) {
            if (x->func->is_pure)
                replace(x);
        } else if_a (GEPInst, x, i)
            replace(x);
        else if_a (PhiInst, x, i) { // TODO: undef
            auto &vals = x->vals;
            asserts(!vals.empty());
            Value *rt = get(vals.front().first.value);
            bool ok = vals.size() == 1;
            if (!ok) {
                ok = true;
                for (uint j = 1; j < vals.size(); ++j)
                    if (rt != get(vals[j].first.value)) {
                        ok = false;
                        break;
                    }
            }
            if (ok)
                replace(i, rt);
        }
    }

    void gvn(Func *f) {
        FOR_BB (bb, *f)
            bb->vis = false;
        vector<BB *> po;
        build_po(f->bbs.front, po);  // TODO: will this require dbe?
        for (auto it = po.rbegin(); it != po.rend(); ++it)  // rpo
            FOR_LIST_MUT (i, (*it)->insts)
                check(i);
    }
};

template <>
Value *GVN::find_a(BinaryInst *k) {
    auto op = k->op;
    auto lh = get(k->lhs.value), rh = get(k->rhs.value); // FIXME: lazy_get causes infinite recursion
    for (uint i = 0; i < vn.size(); ++i) {
        auto p = vn[i];
        if_a (BinaryInst, x, p.first) {
            auto o = x->op;
            auto *l = get(x->lhs.value), *r = get(x->rhs.value);
            if ((lh == l && rh == r && op == o) ||
                (lh == r && rh == l && BinaryInst::is_op_mirror(op, o))
                )
                return p.second;  // ref could have been invalid
        }
    }
    return k;
}

template <>
Value *GVN::find_a(CallInst *k) {
    auto *f = k->func;
    if (!f->is_pure)
        return k;
    vector<Value *> args;
    for (auto &u: k->args)
        args.push_back(get(u.value));
    for (uint i = 0; i < vn.size(); ++i) {
        auto p = vn[i];
        if_a (CallInst, x, p.first) if (x->func == f) {
            bool ok = true;
            uint n = args.size();
            for (uint j = 0; j < n; ++j) {
                if (args[j] != get(x->args[j].value)) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return p.second;
        }
    }
    return k;
}

template <>
Value *GVN::find_a(GEPInst *k) {
    auto base = get(k->base.value), off = get(k->off.value);
    for (uint i = 0; i < vn.size(); ++i) {
        auto p = vn[i];
        if_a (GEPInst, x, p.first) {
            if (get(x->base.value) == base && get(x->off.value) == off)
                return p.second;
        }
    }
    return k;
}

Value *GVN::get(Value *i) {
    for (auto &p: vn) if (p.first == i)
        return p.second;
    Value *v;
    if_a (BinaryInst, x, i) v = find_a(x);
    else if_a (CallInst, x, i) v = find_a(x);
    else if_a (GEPInst, x, i) v = find_a(x);
    else v = i;
    vn.emplace_back(i, v);
    return v;
}

static bool is_pinned(Inst *i) {
    if_a (CallInst, x, i)
        return !x->func->is_pure;  // Will infinite loops be promoted?
    return i->has_side_effects() || is_a<PhiInst>(i) || is_a<LoadInst>(i) || is_a<AllocaInst>(i);
}

static void schedule_early(Inst *i, BB *root) {
    if (i->vis)
        return;
    i->vis = true;
    auto *bb = root;
    for (auto *u: get_owned_uses(i))
        if_a (Inst, x, u->value) {
            schedule_early(x, root);
            if (x->bb->dom_depth > bb->dom_depth)
                bb = x->bb;
        }
    if (bb != i->bb) {
        i->bb->erase(i);
        bb->insts.insert(bb->insts.back, i);
        i->bb = bb;
    }
}

static BB *find_lca(BB *u, BB *v) {
    while (u->dom_depth > v->dom_depth)
        u = u->idom;
    while (v->dom_depth > u->dom_depth)
        v = v->idom;
    while (u != v) {
        u = u->idom;
        v = v->idom;
    }
    return u;
}

static int loop_depth(BB *u) {
    int d = u->loop ? u->loop->depth : 0;
    infof("loop depth of bb", u->id, "is", d);
    return d;
}

static void schedule_late(Inst *i) {
    if (i->vis)
        return;
    i->vis = true;
    BB *lca = nullptr;

    std::set<Inst *> users;
    FOR_LIST (u, i->uses)
        if_a (Inst, x, u->user) {
            users.insert(x);
            schedule_late(x);
            BB *ubb;
            if_a (PhiInst, y, x) {
                bool ok = false;
                for (auto &p: y->vals)
                    if (&p.first == u) {  // Multiple uses of phi can have the same value
                        ubb = p.second;
                        ok = true;
                        break;
                    }
                asserts(ok);
            } else
                ubb = x->bb;
            lca = lca ? find_lca(lca, ubb) : ubb;
        }
    asserts(lca);

    auto *bb = lca;
    while (lca != i->bb) {
        lca = lca->idom;
        if (loop_depth(lca) < loop_depth(bb))
            bb = lca;
    }

    // Reinsert i even if bb != i->bb to order them up after schedule_early
    i->bb->erase(i);
    i->bb = bb;
    FOR_INST (j, *bb)
        if (!is_a<PhiInst>(j))
            if (users.count(j)) {
                bb->insts.insert(j, i);
                return;
            }
    bb->insts.insert(bb->insts.back, i);
}

void gg(Func *f) {
    infof(f->name, "gg");

    build_loop(f);
    GVN().gvn(f);
    dce(f);

    static vector<Inst *> wl;
    wl.clear();
    FOR_BB_INST (i, bb, *f)
        if (!(i->vis = is_pinned(i)))
            wl.push_back(i);
    for (auto *i: wl)
        schedule_early(i, f->bbs.front);

    wl.clear();
    FOR_BB_INST (i, bb, *f)
        if (!(i->vis = is_pinned(i)))
            wl.push_back(i);
    for (auto *i: wl)
        schedule_late(i);
}
