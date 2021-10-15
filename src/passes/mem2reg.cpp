#include "../ir.hpp"

using namespace ir;

static void traverse(BB *u, BB *block) {
    if (u->vis || u == block)
        return;
    u->vis = true;
    for (auto v : u->get_succ())
        traverse(v, block);
}

void build_dom(Func *f) {
    FOR_BB (u, *f) {
        u->dom.clear();
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
            for (BB *v : u->dom) if (v != u && w != v && v->dom.count(w)) {
                ok = false;
                break;
            }
            if (ok) {
                u->idom = w;
                info("%s: bb_%d idoms bb_%d", f->name.data(), w->id, u->id);
                break;
            }
        }
    }
}

void build_df(Func *f) {
    build_dom(f);

    FOR_BB (u, *f) {
        u->pred.clear();
        u->df.clear();
    }

    FOR_BB (u, *f)
        for (BB *v : u->get_succ())
            v->pred.push_back(u);

    // eac
    FOR_BB (u, *f) if (u->pred.size() > 1) {
        for (BB *p : u->pred) {
            for (; p != u->idom; p = p->idom) {
                p->df.push_back(u);
                info("%s: bb_%d has bb_%d as df", f->name.data(), p->id, u->id);
            }
        }
    }
}

// requires dbe
void mem2reg(Func *f) {
    build_df(f);

    vector<AllocaInst *> allocas;
    FOR_BB_INST (i, bb, *f) {
        if_a (AllocaInst, a, i) {
            if (a->var->dims.empty()) { // promotable
                a->aid = int(allocas.size());
                allocas.push_back(a);
                info("found promotable alloca %d for %s", a->aid, a->var->name.data());
            } else
                a->aid = -1;
        }
    }
    uint n = allocas.size();

    {
        auto *def_bbs = new vector<BB *>[n];
        FOR_BB_INST (i, bb, *f)
            if_a (StoreInst, s, i)
                if_a (AllocaInst, a, s->base.value)
                    if (a->aid >= 0) {
                        def_bbs[a->aid].push_back(bb);
                        info("found def bb_%d for alloca %d", bb->id, a->aid);
                    }
        // okay to push the same bb for multiple times due to checking bb->vis

        vector<BB *> wl;
        for (uint i = 0; i < n; ++i) {
            FOR_BB (bb, *f)
                bb->vis = false;
            for (auto *bb : def_bbs[i]) if (!bb->vis)
                wl.push_back(bb); // vis shouldn't be set here, for these bbs hasn't be place phi nodes yet

            while (!wl.empty()) {
                auto *u = wl.back();
                wl.pop_back();
                for (BB *v : u->df) if (!v->vis) {
                    infof("alloca", i, "put phi at bb", v->id, "which is in df of bb", u->id);
                    wl.push_back(v);
                    v->vis = true;
                    v->push_front(new PhiInst)->aid = int(i);
                }
            }
        }
        delete []def_bbs;
    }

    FOR_BB (bb, *f)
        bb->vis = false;
    f->bbs.front->vis = true;
    vector<std::pair<BB *, vector<Value *>>> wl{
        {{f->bbs.front, {n, &Undef::VAL}}}
    };
    do {
        auto &s = wl.back();
        auto *bb = s.first;
        auto vals = std::move(s.second);
        wl.pop_back();
        for (auto *i = bb->insts.front; i; ) {
            auto *next = i->next;
            if_a (AllocaInst, x, i) {
                if (x->aid >= 0)
                    bb->erase(x);
            } else if_a (LoadInst, x, i) {
                if_a (AllocaInst, a, x->base.value) {
                    if (a->aid >= 0) {
                        x->lhs->value = nullptr;  // param codegen uses this!
                        bb->erase_with(x, vals[a->aid]);
                        delete x;
                    }
                }
            } else if_a (StoreInst, x, i) {
                if_a (AllocaInst, a, x->base.value) {
                    if (a->aid >= 0) {
                        x->lhs->value = nullptr;
                        vals[a->aid] = x->val.value; // no need to release()
                        bb->erase(x);
                        delete x;
                    }
                }
            } else if_a (PhiInst, x, i) {
                if (x->aid >= 0)
                    vals[x->aid] = x;
            }
            i = next;
        }
        for (BB *v : bb->get_succ()) {
            FOR_INST (i, *v) {
                if_a (PhiInst, x, i) {
                    if (x->aid >= 0)
                        x->push(vals[x->aid], bb);
                } else
                    break;
            }
            if (!v->vis) {
                v->vis = true;
                wl.emplace_back(v, vals);
            }
        }
    } while (!wl.empty());

    for (auto *a : allocas)
        delete a;

    info("%s: mem2reg done", f->name.data());
}
